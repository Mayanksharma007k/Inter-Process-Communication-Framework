#pragma once

#include "ipc_channel.hpp"
#include <vector>
#include <string>
#include <functional>

struct BenchmarkResult {
    std::string mechanism;
    size_t payload_size;
    size_t iterations;
    double p50_latency_us;
    double p90_latency_us;
    double p99_latency_us;
    double min_latency_us;
    double max_latency_us;
    double avg_latency_us;
    double throughput_gbps;
    double throughput_mbps;
    long voluntary_cs;
    long involuntary_cs;
};

using TelemetryCallback = std::function<void(const std::string& json_line)>;

class BenchmarkSuite {
public:
    static BenchmarkResult run_benchmark(
        IpcType type,
        size_t payload_size,
        size_t iterations,
        TelemetryCallback callback = nullptr
    );

    static std::vector<BenchmarkResult> run_full_suite(
        size_t iterations = 50,
        TelemetryCallback callback = nullptr
    );
};
