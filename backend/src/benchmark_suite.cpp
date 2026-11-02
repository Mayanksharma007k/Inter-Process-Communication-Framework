#include "../include/benchmark_suite.hpp"
#include "../include/pipe_channel.hpp"
#include "../include/shm_channel.hpp"
#include "../include/mq_channel.hpp"
#include "../include/socket_channel.hpp"
#include <unistd.h>
#include <algorithm>
#include <thread>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <iostream>

static std::unique_ptr<IpcChannel> create_channel(IpcType type) {
    switch (type) {
        case IpcType::Pipes: return std::make_unique<PipeChannel>();
        case IpcType::SharedMemory: return std::make_unique<ShmChannel>();
        case IpcType::MessageQueues: return std::make_unique<MqChannel>();
        case IpcType::UnixSockets: return std::make_unique<SocketChannel>();
    }
    return nullptr;
}

BenchmarkResult BenchmarkSuite::run_benchmark(
    IpcType type,
    size_t payload_size,
    size_t iterations,
    TelemetryCallback callback
) {
    std::string channel_name = "bench_" + ipc_type_to_string(type) + "_" + std::to_string(getpid());
    
    // Allocate payload buffer with non-zero test pattern
    std::vector<uint8_t> send_buffer(payload_size);
    for (size_t i = 0; i < payload_size; ++i) {
        send_buffer[i] = static_cast<uint8_t>((i & 0xFF) ^ 0x5A);
    }
    std::vector<uint8_t> recv_buffer(payload_size + 4096);

    std::unique_ptr<IpcChannel> producer = create_channel(type);
    std::unique_ptr<IpcChannel> consumer = create_channel(type);

    ChannelConfig p_conf{channel_name, ChannelRole::Producer, payload_size * 2 + 1024 * 1024};
    ChannelConfig c_conf{channel_name, ChannelRole::Consumer, payload_size * 2 + 1024 * 1024};

    // Consumer initialization in thread to coordinate socket/pipe handshake
    std::thread init_consumer([&]() {
        consumer->initialize(c_conf);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    producer->initialize(p_conf);
    if (init_consumer.joinable()) {
        init_consumer.join();
    }

    std::vector<double> latencies_us;
    latencies_us.reserve(iterations);

    ProcessRusage ru_start = get_current_rusage();
    uint64_t total_start_ns = get_monotonic_time_ns();

    // Consumer thread to receive messages
    for (size_t iter = 0; iter < iterations; ++iter) {
        IpcPacketHeader recv_hdr{};
        uint64_t t0 = get_monotonic_time_ns();

        std::thread receiver([&]() {
            consumer->receive(recv_buffer.data(), recv_buffer.size(), recv_hdr);
        });

        producer->send(send_buffer.data(), payload_size, iter + 1);

        if (receiver.joinable()) {
            receiver.join();
        }
        uint64_t t1 = get_monotonic_time_ns();

        double lat_us = static_cast<double>(t1 - t0) / 1000.0;
        latencies_us.push_back(lat_us);

        if (callback) {
            std::ostringstream ss;
            ss << "{"
               << "\"type\":\"packet\","
               << "\"seq\":" << (iter + 1) << ","
               << "\"ipc\":\"" << ipc_type_to_string(type) << "\","
               << "\"size_bytes\":" << payload_size << ","
               << "\"latency_us\":" << std::fixed << std::setprecision(2) << lat_us << ","
               << "\"checksum\":\"0x" << std::hex << recv_hdr.crc32 << std::dec << "\","
               << "\"source_pid\":" << getpid() << ","
               << "\"target_pid\":" << (getpid() + 1)
               << "}";
            callback(ss.str());
        }
    }

    uint64_t total_end_ns = get_monotonic_time_ns();
    ProcessRusage ru_end = get_current_rusage();

    producer->close();
    consumer->close();
    producer->teardown();

    // Calculate percentiles
    std::sort(latencies_us.begin(), latencies_us.end());
    double min_lat = latencies_us.empty() ? 0 : latencies_us.front();
    double max_lat = latencies_us.empty() ? 0 : latencies_us.back();
    double p50 = latencies_us.empty() ? 0 : latencies_us[latencies_us.size() * 50 / 100];
    double p90 = latencies_us.empty() ? 0 : latencies_us[latencies_us.size() * 90 / 100];
    double p99 = latencies_us.empty() ? 0 : latencies_us[latencies_us.size() * 99 / 100];
    double avg = latencies_us.empty() ? 0 : std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0) / latencies_us.size();

    double total_sec = static_cast<double>(total_end_ns - total_start_ns) / 1e9;
    double total_bytes = static_cast<double>(payload_size * iterations);
    double mbps = (total_sec > 0) ? (total_bytes / (1024.0 * 1024.0)) / total_sec : 0;
    double gbps = mbps / 1024.0;

    BenchmarkResult res;
    res.mechanism = ipc_type_to_string(type);
    res.payload_size = payload_size;
    res.iterations = iterations;
    res.p50_latency_us = p50;
    res.p90_latency_us = p90;
    res.p99_latency_us = p99;
    res.min_latency_us = min_lat;
    res.max_latency_us = max_lat;
    res.avg_latency_us = avg;
    res.throughput_gbps = gbps;
    res.throughput_mbps = mbps;
    res.voluntary_cs = ru_end.voluntary_cs - ru_start.voluntary_cs;
    res.involuntary_cs = ru_end.involuntary_cs - ru_start.involuntary_cs;

    return res;
}

std::vector<BenchmarkResult> BenchmarkSuite::run_full_suite(
    size_t iterations,
    TelemetryCallback callback
) {
    std::vector<BenchmarkResult> results;
    std::vector<IpcType> types = {
        IpcType::SharedMemory,
        IpcType::UnixSockets,
        IpcType::Pipes,
        IpcType::MessageQueues
    };

    // Test matrix: 1KB, 1MB, 10MB
    std::vector<size_t> sizes = {
        1024,
        1024 * 1024,
        10 * 1024 * 1024
    };

    for (auto type : types) {
        for (auto sz : sizes) {
            size_t iters = (sz >= 10 * 1024 * 1024) ? std::min<size_t>(iterations, 10) : iterations;
            results.push_back(run_benchmark(type, sz, iters, callback));
        }
    }

    return results;
}
