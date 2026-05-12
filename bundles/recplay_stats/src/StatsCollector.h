// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IStatsService.h"

#include <atomic>
#include <functional>
#include <mutex>

namespace recplay {

class StatsCollector final : public IStatsService {
public:
    StatsSnapshot GetSnapshot() const override;
    void OnUpdate(std::function<void(const StatsSnapshot&)> cb) override;
    void RecordPacket(const std::string& protocol, uint32_t channelId, size_t bytes) override;
    void RecordDrop(const std::string& protocol, uint64_t count = 1) override;
    void RecordWriteLatency(double ms) override;
    void UpdateRingBufferState(uint32_t used, uint32_t capacity) override;
    void UpdateDiskQueue(uint64_t bytes) override;

private:
    mutable std::mutex mutex_;
    StatsSnapshot snapshot_;
    std::function<void(const StatsSnapshot&)> callback_;
};

} // namespace recplay
