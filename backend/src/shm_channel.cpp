#include "../include/shm_channel.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

ShmChannel::ShmChannel() = default;

ShmChannel::~ShmChannel() {
    close();
}

bool ShmChannel::initialize(const ChannelConfig& config) {
    name_ = config.channel_name;
    role_ = config.role;
    shm_path_ = "/omni_shm_" + name_;

    size_t payload_capacity = config.buffer_capacity;
    total_mapped_size_ = sizeof(ShmControlBlock) + payload_capacity;

    int oflag = O_RDWR;
    if (role_ == ChannelRole::Producer) {
        oflag |= O_CREAT;
    }

    shm_fd_ = shm_open(shm_path_.c_str(), oflag, 0666);
    if (shm_fd_ < 0) {
        // Fallback retry with create if consumer started first
        shm_fd_ = shm_open(shm_path_.c_str(), O_RDWR | O_CREAT, 0666);
        if (shm_fd_ < 0) return false;
    }

    if (role_ == ChannelRole::Producer) {
        if (ftruncate(shm_fd_, total_mapped_size_) != 0) {
            ::close(shm_fd_);
            return false;
        }
    }

    mapped_ptr_ = mmap(nullptr, total_mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (mapped_ptr_ == MAP_FAILED) {
        ::close(shm_fd_);
        mapped_ptr_ = nullptr;
        return false;
    }

    ctrl_ = reinterpret_cast<ShmControlBlock*>(mapped_ptr_);
    ring_data_ = reinterpret_cast<uint8_t*>(mapped_ptr_) + sizeof(ShmControlBlock);

    if (role_ == ChannelRole::Producer) {
        pthread_mutexattr_t mattr;
        pthread_mutexattr_init(&mattr);
        pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&ctrl_->mutex, &mattr);
        pthread_mutexattr_destroy(&mattr);

        pthread_condattr_t cattr;
        pthread_condattr_init(&cattr);
        pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&ctrl_->cond_has_data, &cattr);
        pthread_cond_init(&ctrl_->cond_has_space, &cattr);
        pthread_condattr_destroy(&cattr);

        ctrl_->head = 0;
        ctrl_->tail = 0;
        ctrl_->capacity = payload_capacity;
        ctrl_->active_producers = 1;
        ctrl_->active_consumers = 0;
        ctrl_->is_closed = false;
    } else {
        pthread_mutex_lock(&ctrl_->mutex);
        ctrl_->active_consumers++;
        pthread_mutex_unlock(&ctrl_->mutex);
    }

    return true;
}

void ShmChannel::close() {
    if (ctrl_) {
        pthread_mutex_lock(&ctrl_->mutex);
        if (role_ == ChannelRole::Producer) {
            ctrl_->is_closed = true;
            pthread_cond_broadcast(&ctrl_->cond_has_data);
        }
        pthread_mutex_unlock(&ctrl_->mutex);
    }

    if (mapped_ptr_ && mapped_ptr_ != MAP_FAILED) {
        munmap(mapped_ptr_, total_mapped_size_);
        mapped_ptr_ = nullptr;
        ctrl_ = nullptr;
        ring_data_ = nullptr;
    }

    if (shm_fd_ >= 0) {
        ::close(shm_fd_);
        shm_fd_ = -1;
    }
}

void ShmChannel::teardown() {
    close();
    shm_unlink(shm_path_.c_str());
}

bool ShmChannel::send(const void* data, size_t size, uint64_t sequence_id) {
    if (!ctrl_) return false;

    IpcPacketHeader header;
    header.sequence_id = sequence_id;
    header.sender_pid = getpid();
    header.receiver_pid = 0;
    header.payload_size = static_cast<uint32_t>(size);
    header.timestamp_ns = get_monotonic_time_ns();
    header.crc32 = calculate_crc32(reinterpret_cast<const uint8_t*>(data), size);

    size_t total_write = sizeof(IpcPacketHeader) + size;

    pthread_mutex_lock(&ctrl_->mutex);

    // Wait for space
    while ((ctrl_->capacity - (ctrl_->head - ctrl_->tail)) < total_write && !ctrl_->is_closed) {
        pthread_cond_wait(&ctrl_->cond_has_space, &ctrl_->mutex);
    }

    if (ctrl_->is_closed) {
        pthread_mutex_unlock(&ctrl_->mutex);
        return false;
    }

    // Write header
    size_t offset = ctrl_->head % ctrl_->capacity;
    if (offset + sizeof(IpcPacketHeader) <= ctrl_->capacity) {
        std::memcpy(ring_data_ + offset, &header, sizeof(IpcPacketHeader));
    } else {
        size_t part1 = ctrl_->capacity - offset;
        std::memcpy(ring_data_ + offset, &header, part1);
        std::memcpy(ring_data_, reinterpret_cast<uint8_t*>(&header) + part1, sizeof(IpcPacketHeader) - part1);
    }
    ctrl_->head += sizeof(IpcPacketHeader);

    // Write payload
    offset = ctrl_->head % ctrl_->capacity;
    if (offset + size <= ctrl_->capacity) {
        std::memcpy(ring_data_ + offset, data, size);
    } else {
        size_t part1 = ctrl_->capacity - offset;
        std::memcpy(ring_data_ + offset, data, part1);
        std::memcpy(ring_data_, reinterpret_cast<const uint8_t*>(data) + part1, size - part1);
    }
    ctrl_->head += size;

    pthread_cond_signal(&ctrl_->cond_has_data);
    pthread_mutex_unlock(&ctrl_->mutex);

    stats_.bytes_transferred += total_write;
    stats_.packets_transferred += 1;
    return true;
}

ssize_t ShmChannel::receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) {
    if (!ctrl_) return -1;

    pthread_mutex_lock(&ctrl_->mutex);

    // Wait until header is available
    while ((ctrl_->head - ctrl_->tail) < sizeof(IpcPacketHeader) && !ctrl_->is_closed) {
        pthread_cond_wait(&ctrl_->cond_has_data, &ctrl_->mutex);
    }

    if ((ctrl_->head - ctrl_->tail) < sizeof(IpcPacketHeader)) {
        pthread_mutex_unlock(&ctrl_->mutex);
        return -1; // Closed
    }

    // Read header
    size_t offset = ctrl_->tail % ctrl_->capacity;
    if (offset + sizeof(IpcPacketHeader) <= ctrl_->capacity) {
        std::memcpy(&out_header, ring_data_ + offset, sizeof(IpcPacketHeader));
    } else {
        size_t part1 = ctrl_->capacity - offset;
        std::memcpy(&out_header, ring_data_ + offset, part1);
        std::memcpy(reinterpret_cast<uint8_t*>(&out_header) + part1, ring_data_, sizeof(IpcPacketHeader) - part1);
    }
    ctrl_->tail += sizeof(IpcPacketHeader);

    if (out_header.magic != 0x49504331) {
        pthread_mutex_unlock(&ctrl_->mutex);
        return -2;
    }

    size_t payload_len = out_header.payload_size;
    if (payload_len > max_size) {
        pthread_mutex_unlock(&ctrl_->mutex);
        return -3;
    }

    // Wait for remaining payload if not yet available
    while ((ctrl_->head - ctrl_->tail) < payload_len && !ctrl_->is_closed) {
        pthread_cond_wait(&ctrl_->cond_has_data, &ctrl_->mutex);
    }

    // Read payload
    offset = ctrl_->tail % ctrl_->capacity;
    if (offset + payload_len <= ctrl_->capacity) {
        std::memcpy(out_buffer, ring_data_ + offset, payload_len);
    } else {
        size_t part1 = ctrl_->capacity - offset;
        std::memcpy(out_buffer, ring_data_ + offset, part1);
        std::memcpy(reinterpret_cast<uint8_t*>(out_buffer) + part1, ring_data_, payload_len - part1);
    }
    ctrl_->tail += payload_len;

    pthread_cond_signal(&ctrl_->cond_has_space);
    pthread_mutex_unlock(&ctrl_->mutex);

    stats_.bytes_transferred += (sizeof(IpcPacketHeader) + payload_len);
    stats_.packets_transferred += 1;
    return static_cast<ssize_t>(payload_len);
}

void* ShmChannel::acquire_write_slot(size_t size) {
    if (!ctrl_ || size > ctrl_->capacity) return nullptr;
    pthread_mutex_lock(&ctrl_->mutex);
    size_t offset = ctrl_->head % ctrl_->capacity;
    if (offset + size <= ctrl_->capacity) {
        return ring_data_ + offset;
    }
    pthread_mutex_unlock(&ctrl_->mutex);
    return nullptr;
}

void ShmChannel::commit_write_slot(size_t size, uint64_t /*seq*/) {
    if (!ctrl_) return;
    ctrl_->head += size;
    pthread_cond_signal(&ctrl_->cond_has_data);
    pthread_mutex_unlock(&ctrl_->mutex);
}

void ShmChannel::reset_stats() {
    stats_ = TransmissionStats{};
}
