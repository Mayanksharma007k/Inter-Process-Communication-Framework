#include "../include/benchmark_suite.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>

static void print_table_header() {
    std::cout << "\n"
              << std::left << std::setw(16) << "Mechanism"
              << std::right << std::setw(12) << "Payload"
              << std::setw(8)  << "Iters"
              << std::setw(12) << "P50 (us)"
              << std::setw(12) << "P99 (us)"
              << std::setw(14) << "MB/s"
              << std::setw(14) << "GB/s"
              << std::setw(12) << "Vol CtxSw"
              << "\n"
              << std::string(90, '-') << "\n";
}

static void print_table_row(const BenchmarkResult& r) {
    std::string size_str;
    if (r.payload_size >= 1024 * 1024) {
        size_str = std::to_string(r.payload_size / (1024 * 1024)) + " MB";
    } else {
        size_str = std::to_string(r.payload_size / 1024) + " KB";
    }

    std::cout << std::left << std::setw(16) << r.mechanism
              << std::right << std::setw(12) << size_str
              << std::setw(8)  << r.iterations
              << std::fixed << std::setprecision(1)
              << std::setw(12) << r.p50_latency_us
              << std::setw(12) << r.p99_latency_us
              << std::setprecision(2)
              << std::setw(14) << r.throughput_mbps
              << std::setw(14) << r.throughput_gbps
              << std::setw(12) << r.voluntary_cs
              << "\n";
}

int main(int argc, char* argv[]) {
    std::string mech = "all";
    size_t payload_size = 1024 * 1024; // 1MB default
    size_t iterations = 20;
    bool stream_json = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mechanism") == 0 && i + 1 < argc) {
            mech = argv[++i];
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "1KB") payload_size = 1024;
            else if (s == "1MB") payload_size = 1024 * 1024;
            else if (s == "10MB") payload_size = 10 * 1024 * 1024;
            else if (s == "100MB") payload_size = 100 * 1024 * 1024;
            else payload_size = std::stoull(s);
        } else if (std::strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = std::stoul(argv[++i]);
        } else if (std::strcmp(argv[i], "--stream-json") == 0) {
            stream_json = true;
        }
    }

    auto callback = [stream_json](const std::string& line) {
        if (stream_json) {
            std::cout << line << std::endl;
        }
    };

    if (stream_json) {
        std::cout << "{\"type\":\"status\",\"msg\":\"Benchmark engine started\"}" << std::endl;
    } else {
        std::cout << "========================================================================\n"
                  << "        OmniIPC Benchmark Harness (POSIX / macOS / Linux Engine)        \n"
                  << "========================================================================\n";
    }

    if (mech == "all") {
        if (!stream_json) print_table_header();
        auto results = BenchmarkSuite::run_full_suite(iterations, callback);
        if (!stream_json) {
            for (const auto& r : results) {
                print_table_row(r);
            }
            std::cout << std::string(90, '=') << "\n";
        }
    } else {
        IpcType type = string_to_ipc_type(mech);
        if (!stream_json) print_table_header();
        auto result = BenchmarkSuite::run_benchmark(type, payload_size, iterations, callback);
        if (!stream_json) {
            print_table_row(result);
            std::cout << std::string(90, '=') << "\n";
        }
    }

    return 0;
}
