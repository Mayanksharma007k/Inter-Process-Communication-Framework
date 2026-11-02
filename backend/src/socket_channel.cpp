#include "../include/socket_channel.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

// Shared storage for local benchmarking socket pairs
static int g_socket_pair[2] = {-1, -1};

SocketChannel::SocketChannel() = default;

SocketChannel::~SocketChannel() {
    close();
}

bool SocketChannel::initialize(const ChannelConfig& config) {
    name_ = config.channel_name;
    role_ = config.role;

    // Use full duplex socketpair for high-speed deterministic IPC
    if (role_ == ChannelRole::Consumer) {
        if (g_socket_pair[0] >= 0) {
            ::close(g_socket_pair[0]);
            ::close(g_socket_pair[1]);
        }
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, g_socket_pair) != 0) {
            return false;
        }

        int buf_size = 8 * 1024 * 1024;
        setsockopt(g_socket_pair[0], SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
        setsockopt(g_socket_pair[0], SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
        setsockopt(g_socket_pair[1], SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
        setsockopt(g_socket_pair[1], SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

        client_fd_ = g_socket_pair[1]; // Consumer end
    } else {
        // Producer end
        for (int i = 0; i < 50; ++i) {
            if (g_socket_pair[0] >= 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        client_fd_ = g_socket_pair[0];
    }

    return (client_fd_ >= 0);
}

void SocketChannel::close() {
    client_fd_ = -1;
}

void SocketChannel::teardown() {
    if (g_socket_pair[0] >= 0) {
        ::close(g_socket_pair[0]);
        g_socket_pair[0] = -1;
    }
    if (g_socket_pair[1] >= 0) {
        ::close(g_socket_pair[1]);
        g_socket_pair[1] = -1;
    }
    client_fd_ = -1;
}

bool SocketChannel::send(const void* data, size_t size, uint64_t sequence_id) {
    if (client_fd_ < 0) return false;

    IpcPacketHeader header;
    header.sequence_id = sequence_id;
    header.sender_pid = getpid();
    header.receiver_pid = 0;
    header.payload_size = static_cast<uint32_t>(size);
    header.timestamp_ns = get_monotonic_time_ns();
    header.crc32 = calculate_crc32(reinterpret_cast<const uint8_t*>(data), size);

    // Send Header
    const uint8_t* h_ptr = reinterpret_cast<const uint8_t*>(&header);
    size_t h_written = 0;
    while (h_written < sizeof(header)) {
        ssize_t w = ::send(client_fd_, h_ptr + h_written, sizeof(header) - h_written, 0);
        if (w <= 0) return false;
        h_written += w;
    }

    // Send Payload
    const uint8_t* d_ptr = reinterpret_cast<const uint8_t*>(data);
    size_t d_written = 0;
    while (d_written < size) {
        ssize_t w = ::send(client_fd_, d_ptr + d_written, size - d_written, 0);
        if (w <= 0) return false;
        d_written += w;
    }

    stats_.bytes_transferred += (sizeof(header) + size);
    stats_.packets_transferred += 1;
    return true;
}

ssize_t SocketChannel::receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) {
    if (client_fd_ < 0) return -1;

    // Receive Header
    uint8_t* h_ptr = reinterpret_cast<uint8_t*>(&out_header);
    size_t h_read = 0;
    while (h_read < sizeof(IpcPacketHeader)) {
        ssize_t r = ::recv(client_fd_, h_ptr + h_read, sizeof(IpcPacketHeader) - h_read, 0);
        if (r <= 0) return -1;
        h_read += r;
    }

    if (out_header.magic != 0x49504331) {
        return -2;
    }

    size_t payload_len = out_header.payload_size;
    if (payload_len > max_size) {
        return -3;
    }

    // Receive Payload
    uint8_t* d_ptr = reinterpret_cast<uint8_t*>(out_buffer);
    size_t d_read = 0;
    while (d_read < payload_len) {
        ssize_t r = ::recv(client_fd_, d_ptr + d_read, payload_len - d_read, 0);
        if (r <= 0) return -1;
        d_read += r;
    }

    stats_.bytes_transferred += (sizeof(IpcPacketHeader) + d_read);
    stats_.packets_transferred += 1;
    return static_cast<ssize_t>(d_read);
}

void SocketChannel::reset_stats() {
    stats_ = TransmissionStats{};
}
