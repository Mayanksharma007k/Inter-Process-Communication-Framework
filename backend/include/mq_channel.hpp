#pragma once

#include "ipc_channel.hpp"
#include <string>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

constexpr size_t SYSV_CHUNK_SIZE = 4096;

struct MqChunkMessage {
    long mtype; // 1 for P2C, 2 for C2P
    uint32_t chunk_index;
    uint32_t total_chunks;
    uint32_t chunk_length;
    uint8_t data[SYSV_CHUNK_SIZE];
};

class MqChannel : public IpcChannel {
public:
    MqChannel();
    ~MqChannel() override;

    bool initialize(const ChannelConfig& config) override;
    void close() override;
    void teardown() override;

    bool send(const void* data, size_t size, uint64_t sequence_id) override;
    ssize_t receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) override;

    IpcType get_type() const noexcept override { return IpcType::MessageQueues; }
    const std::string& get_name() const noexcept override { return name_; }
    TransmissionStats get_stats() const noexcept override { return stats_; }
    void reset_stats() override;

private:
    std::string name_;
    ChannelRole role_;
    key_t ipc_key_ = -1;
    int msg_id_ = -1;
    TransmissionStats stats_;
};
