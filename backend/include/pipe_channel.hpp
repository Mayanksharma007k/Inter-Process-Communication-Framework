#pragma once

#include "ipc_channel.hpp"
#include <string>

class PipeChannel : public IpcChannel {
public:
    PipeChannel();
    ~PipeChannel() override;

    bool initialize(const ChannelConfig& config) override;
    void close() override;
    void teardown() override;

    bool send(const void* data, size_t size, uint64_t sequence_id) override;
    ssize_t receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) override;

    IpcType get_type() const noexcept override { return IpcType::Pipes; }
    const std::string& get_name() const noexcept override { return name_; }
    TransmissionStats get_stats() const noexcept override { return stats_; }
    void reset_stats() override;

private:
    std::string name_;
    ChannelRole role_;
    std::string fifo_p2c_path_;
    std::string fifo_c2p_path_;
    int read_fd_ = -1;
    int write_fd_ = -1;
    TransmissionStats stats_;
};
