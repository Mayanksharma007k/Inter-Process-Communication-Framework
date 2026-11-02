#include "../include/pipe_channel.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

static int g_pipe_p2c[2] = {-1, -1};
static int g_pipe_c2p[2] = {-1, -1};

PipeChannel::PipeChannel() = default;

PipeChannel::~PipeChannel() {
    close();
}

bool PipeChannel::initialize(const ChannelConfig& config) {
    name_ = config.channel_name;
    role_ = config.role;

    if (role_ == ChannelRole::Consumer) {
        if (g_pipe_p2c[0] >= 0) { ::close(g_pipe_p2c[0]); ::close(g_pipe_p2c[1]); }
        if (g_pipe_c2p[0] >= 0) { ::close(g_pipe_c2p[0]); ::close(g_pipe_c2p[1]); }

        if (pipe(g_pipe_p2c) != 0 || pipe(g_pipe_c2p) != 0) {
            return false;
        }

        read_fd_ = g_pipe_p2c[0];
        write_fd_ = g_pipe_c2p[1];
    } else {
        for (int i = 0; i < 50; ++i) {
            if (g_pipe_p2c[1] >= 0 && g_pipe_c2p[0] >= 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        write_fd_ = g_pipe_p2c[1];
        read_fd_ = g_pipe_c2p[0];
    }

    return (read_fd_ >= 0 && write_fd_ >= 0);
}

void PipeChannel::close() {
    read_fd_ = -1;
    write_fd_ = -1;
}

void PipeChannel::teardown() {
    if (g_pipe_p2c[0] >= 0) { ::close(g_pipe_p2c[0]); ::close(g_pipe_p2c[1]); g_pipe_p2c[0] = -1; g_pipe_p2c[1] = -1; }
    if (g_pipe_c2p[0] >= 0) { ::close(g_pipe_c2p[0]); ::close(g_pipe_c2p[1]); g_pipe_c2p[0] = -1; g_pipe_c2p[1] = -1; }
    close();
}

bool PipeChannel::send(const void* data, size_t size, uint64_t sequence_id) {
    if (write_fd_ < 0) return false;

    IpcPacketHeader header;
    header.sequence_id = sequence_id;
    header.sender_pid = getpid();
    header.receiver_pid = 0;
    header.payload_size = static_cast<uint32_t>(size);
    header.timestamp_ns = get_monotonic_time_ns();
    header.crc32 = calculate_crc32(reinterpret_cast<const uint8_t*>(data), size);

    // Write header
    const uint8_t* h_ptr = reinterpret_cast<const uint8_t*>(&header);
    size_t h_written = 0;
    while (h_written < sizeof(header)) {
        ssize_t w = ::write(write_fd_, h_ptr + h_written, sizeof(header) - h_written);
        if (w <= 0) return false;
        h_written += w;
    }

    // Write payload
    const uint8_t* d_ptr = reinterpret_cast<const uint8_t*>(data);
    size_t d_written = 0;
    while (d_written < size) {
        ssize_t w = ::write(write_fd_, d_ptr + d_written, size - d_written);
        if (w <= 0) return false;
        d_written += w;
    }

    stats_.bytes_transferred += (sizeof(header) + size);
    stats_.packets_transferred += 1;
    return true;
}

ssize_t PipeChannel::receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) {
    if (read_fd_ < 0) return -1;

    // Read header
    uint8_t* h_ptr = reinterpret_cast<uint8_t*>(&out_header);
    size_t h_read = 0;
    while (h_read < sizeof(IpcPacketHeader)) {
        ssize_t r = ::read(read_fd_, h_ptr + h_read, sizeof(IpcPacketHeader) - h_read);
        if (r <= 0) return -1;
        h_read += r;
    }

    if (out_header.magic != 0x49504331) {
        return -2; // Bad magic
    }

    size_t to_read = out_header.payload_size;
    if (to_read > max_size) return -3; // Buffer too small

    uint8_t* d_ptr = reinterpret_cast<uint8_t*>(out_buffer);
    size_t d_read = 0;
    while (d_read < to_read) {
        ssize_t r = ::read(read_fd_, d_ptr + d_read, to_read - d_read);
        if (r <= 0) return -1;
        d_read += r;
    }

    stats_.bytes_transferred += (sizeof(IpcPacketHeader) + d_read);
    stats_.packets_transferred += 1;
    return static_cast<ssize_t>(d_read);
}

void PipeChannel::reset_stats() {
    stats_ = TransmissionStats{};
}
