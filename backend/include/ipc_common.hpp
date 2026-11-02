#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <chrono>
#include <sys/resource.h>
#include <sys/time.h>
#include <vector>

#pragma pack(push, 1)
struct IpcPacketHeader {
    uint32_t magic = 0x49504331; // "IPC1"
    uint64_t sequence_id;
    uint32_t sender_pid;
    uint32_t receiver_pid;
    uint32_t payload_size;
    uint64_t timestamp_ns;       // CLOCK_MONOTONIC timestamp
    uint32_t crc32;              // Payload integrity check
};
#pragma pack(pop)

static_assert(sizeof(IpcPacketHeader) == 36, "IpcPacketHeader must be 36 bytes packed");

enum class IpcType {
    Pipes,
    SharedMemory,
    MessageQueues,
    UnixSockets
};

inline std::string ipc_type_to_string(IpcType type) {
    switch (type) {
        case IpcType::Pipes: return "pipes";
        case IpcType::SharedMemory: return "shared-memory";
        case IpcType::MessageQueues: return "message-queues";
        case IpcType::UnixSockets: return "sockets";
    }
    return "unknown";
}

inline IpcType string_to_ipc_type(const std::string& str) {
    if (str == "pipes") return IpcType::Pipes;
    if (str == "shared-memory" || str == "shm") return IpcType::SharedMemory;
    if (str == "message-queues" || str == "mq") return IpcType::MessageQueues;
    return IpcType::UnixSockets;
}

enum class ChannelRole {
    Producer,
    Consumer
};

struct ChannelConfig {
    std::string channel_name = "omni_ipc_default";
    ChannelRole role = ChannelRole::Producer;
    size_t buffer_capacity = 64 * 1024 * 1024; // 64 MB
    bool non_blocking = false;
    uint32_t timeout_ms = 5000;
};

struct TransmissionStats {
    uint64_t bytes_transferred = 0;
    uint64_t packets_transferred = 0;
    std::chrono::nanoseconds total_duration{0};
    uint64_t voluntary_context_switches = 0;
    uint64_t involuntary_context_switches = 0;
    double p50_latency_us = 0.0;
    double p90_latency_us = 0.0;
    double p99_latency_us = 0.0;
    double min_latency_us = 0.0;
    double max_latency_us = 0.0;
    double throughput_gbps = 0.0;
};

// Monotonic nanosecond timestamp
inline uint64_t get_monotonic_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

// Fast CRC32 Implementation
inline uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    static const uint32_t crc_table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };

    uint32_t crc = ~0U;
    for (size_t i = 0; i < length; ++i) {
        crc = crc_table[(crc ^ data[i]) & 0x0F] ^ (crc >> 4);
        crc = crc_table[(crc ^ (data[i] >> 4)) & 0x0F] ^ (crc >> 4);
    }
    return ~crc;
}

// Context switch counter using getrusage
struct ProcessRusage {
    long voluntary_cs;
    long involuntary_cs;
};

inline ProcessRusage get_current_rusage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return { usage.ru_nvcsw, usage.ru_nivcsw };
}
