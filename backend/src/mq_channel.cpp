#include "../include/mq_channel.hpp"
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cmath>

struct SysvMsgBuf {
    long mtype;
    uint8_t buffer[SYSV_CHUNK_SIZE + sizeof(IpcPacketHeader) + 16];
};

MqChannel::MqChannel() = default;

MqChannel::~MqChannel() {
    close();
}

bool MqChannel::initialize(const ChannelConfig& config) {
    name_ = config.channel_name;
    role_ = config.role;

    // Generate numeric key from name hash
    uint32_t hash = 5381;
    for (char c : name_) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
    }
    ipc_key_ = static_cast<key_t>(0x1000 + (hash % 0xE000));

    int flags = 0666;
    if (role_ == ChannelRole::Producer) {
        flags |= IPC_CREAT;
    }

    msg_id_ = msgget(ipc_key_, flags);
    if (msg_id_ < 0) {
        msg_id_ = msgget(ipc_key_, IPC_CREAT | 0666);
        if (msg_id_ < 0) return false;
    }

    return true;
}

void MqChannel::close() {
    // SysV queues persist until IPC_RMID
}

void MqChannel::teardown() {
    if (msg_id_ >= 0) {
        msgctl(msg_id_, IPC_RMID, nullptr);
        msg_id_ = -1;
    }
}

bool MqChannel::send(const void* data, size_t size, uint64_t sequence_id) {
    if (msg_id_ < 0) return false;

    IpcPacketHeader header;
    header.sequence_id = sequence_id;
    header.sender_pid = getpid();
    header.receiver_pid = 0;
    header.payload_size = static_cast<uint32_t>(size);
    header.timestamp_ns = get_monotonic_time_ns();
    header.crc32 = calculate_crc32(reinterpret_cast<const uint8_t*>(data), size);

    // Send Header packet first
    SysvMsgBuf h_msg;
    h_msg.mtype = (role_ == ChannelRole::Producer) ? 1 : 2;
    std::memcpy(h_msg.buffer, &header, sizeof(IpcPacketHeader));
    if (msgsnd(msg_id_, &h_msg, sizeof(IpcPacketHeader), 0) != 0) {
        return false;
    }

    // Send payload chunks
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk_len = (remaining > SYSV_CHUNK_SIZE) ? SYSV_CHUNK_SIZE : remaining;
        SysvMsgBuf c_msg;
        c_msg.mtype = (role_ == ChannelRole::Producer) ? 1 : 2;
        std::memcpy(c_msg.buffer, ptr, chunk_len);

        if (msgsnd(msg_id_, &c_msg, chunk_len, 0) != 0) {
            return false;
        }

        ptr += chunk_len;
        remaining -= chunk_len;
    }

    stats_.bytes_transferred += (sizeof(IpcPacketHeader) + size);
    stats_.packets_transferred += 1;
    return true;
}

ssize_t MqChannel::receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) {
    if (msg_id_ < 0) return -1;

    long target_mtype = (role_ == ChannelRole::Consumer) ? 1 : 2;

    // Receive Header
    SysvMsgBuf h_msg;
    ssize_t h_bytes = msgrcv(msg_id_, &h_msg, sizeof(IpcPacketHeader), target_mtype, 0);
    if (h_bytes < static_cast<ssize_t>(sizeof(IpcPacketHeader))) {
        return -1;
    }

    std::memcpy(&out_header, h_msg.buffer, sizeof(IpcPacketHeader));
    if (out_header.magic != 0x49504331) {
        return -2;
    }

    size_t payload_len = out_header.payload_size;
    if (payload_len > max_size) {
        return -3;
    }

    // Receive payload chunks
    uint8_t* ptr = reinterpret_cast<uint8_t*>(out_buffer);
    size_t remaining = payload_len;
    while (remaining > 0) {
        SysvMsgBuf c_msg;
        ssize_t r = msgrcv(msg_id_, &c_msg, SYSV_CHUNK_SIZE, target_mtype, 0);
        if (r <= 0) return -1;

        size_t actual_chunk = static_cast<size_t>(r);
        std::memcpy(ptr, c_msg.buffer, actual_chunk);

        ptr += actual_chunk;
        remaining = (remaining >= actual_chunk) ? (remaining - actual_chunk) : 0;
    }

    stats_.bytes_transferred += (sizeof(IpcPacketHeader) + payload_len);
    stats_.packets_transferred += 1;
    return static_cast<ssize_t>(payload_len);
}

void MqChannel::reset_stats() {
    stats_ = TransmissionStats{};
}
