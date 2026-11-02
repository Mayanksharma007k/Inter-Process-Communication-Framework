#pragma once

#include "ipc_common.hpp"
#include <span>
#include <memory>
#include <string>

class IpcChannel {
public:
    virtual ~IpcChannel() = default;

    // Initialization & Lifecycle
    virtual bool initialize(const ChannelConfig& config) = 0;
    virtual void close() = 0;
    virtual void teardown() = 0;

    // Data Transmission
    virtual bool send(const void* data, size_t size, uint64_t sequence_id) = 0;
    virtual ssize_t receive(void* out_buffer, size_t max_size, IpcPacketHeader& out_header) = 0;

    // Zero-Copy Capabilities
    virtual bool supports_zero_copy() const noexcept { return false; }
    virtual void* acquire_write_slot(size_t /*size*/) { return nullptr; }
    virtual void commit_write_slot(size_t /*size*/, uint64_t /*seq*/) {}

    // Introspection
    virtual IpcType get_type() const noexcept = 0;
    virtual const std::string& get_name() const noexcept = 0;
    virtual TransmissionStats get_stats() const noexcept = 0;
    virtual void reset_stats() = 0;
};
