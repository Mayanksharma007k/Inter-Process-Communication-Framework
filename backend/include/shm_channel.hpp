#pragma once

#include "ipc_channel.hpp"
#include <atomic>
#include <pthread.h>
#include <string>

struct alignas(64) ShmControlBlock {
    pthread_mutex_t mutex;
    pthread_cond_t cond_has_data;
    pthread_cond_t cond_has_space;
    uint64_t head;       // Write position
    uint64_t tail;       // Read position
    size_t capacity;     // Payload area capacity
    uint32_t active_producers;
    uint32_t active_consumers;
    bool is_closed;
};

class ShmChannel : public IpcChannel {
public:
    ShmChannel();
    ~ShmChannel() override;

    bool initialize(const ChannelConfig& config) override;
    void close() override;
    void teardown() override;

    bool send(const void* data, size_t size, uint64_t sequence_id) override;
    ssize_t receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) override;

    bool supports_zero_copy() const noexcept override { return true; }
    void* acquire_write_slot(size_t size) override;
    void commit_write_slot(size_t size, uint64_t seq) override;

    IpcType get_type() const noexcept override { return IpcType::SharedMemory; }
    const std::string& get_name() const noexcept override { return name_; }
    TransmissionStats get_stats() const noexcept override { return stats_; }
    void reset_stats() override;

private:
    std::string name_;
    std::string shm_path_;
    ChannelRole role_;
    int shm_fd_ = -1;
    size_t total_mapped_size_ = 0;
    void* mapped_ptr_ = nullptr;
    ShmControlBlock* ctrl_ = nullptr;
    uint8_t* ring_data_ = nullptr;
    TransmissionStats stats_;
};
