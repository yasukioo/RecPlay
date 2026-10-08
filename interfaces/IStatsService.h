// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <cstddef>
#include <functional>
#include <string>

namespace recplay {

class IStatsService {
public:
    virtual ~IStatsService() = default;

    virtual StatsSnapshot GetSnapshot() const = 0;
    virtual void OnUpdate(std::function<void(const StatsSnapshot&)> cb) = 0;
    virtual void RecordPacket(const std::string& protocol, uint32_t channelId, size_t bytes) = 0;
    virtual void RecordDrop(const std::string& protocol, uint64_t count = 1) = 0;
    virtual void RecordWriteLatency(double ms) = 0;
    virtual void UpdateRingBufferState(uint32_t used, uint32_t capacity) = 0;
    virtual void UpdateDiskQueue(uint64_t bytes) = 0;
};

} // namespace recplay
