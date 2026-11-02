#pragma once

#include "ipc_channel.hpp"
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

class SocketChannel : public IpcChannel {
public:
    SocketChannel();
    ~SocketChannel() override;

    bool initialize(const ChannelConfig& config) override;
    void close() override;
    void teardown() override;

    bool send(const void* data, size_t size, uint64_t sequence_id) override;
    ssize_t receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) override;

    IpcType get_type() const noexcept override { return IpcType::UnixSockets; }
    const std::string& get_name() const noexcept override { return name_; }
    TransmissionStats get_stats() const noexcept override { return stats_; }
    void reset_stats() override;

private:
    std::string name_;
    std::string socket_path_;
    ChannelRole role_;
    int server_fd_ = -1;
    int client_fd_ = -1;
    TransmissionStats stats_;
};
