// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IStatsService.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace recplay {

class StatsCollector final : public IStatsService {
public:
    using CpuUsageSampler = std::function<std::optional<double>()>;

    explicit StatsCollector(CpuUsageSampler cpuUsageSampler = {});
    ~StatsCollector() override;

    StatsSnapshot GetSnapshot() const override;
    void OnUpdate(std::function<void(const StatsSnapshot&)> cb) override;
    void RecordPacket(const std::string& protocol, uint32_t channelId, size_t bytes) override;
    void RecordDrop(const std::string& protocol, uint64_t count = 1) override;
    void RecordWriteLatency(double ms) override;
    void UpdateRingBufferState(uint32_t used, uint32_t capacity) override;
    void UpdateDiskQueue(uint64_t bytes) override;

private:
    struct ProtocolAccumulator {
        uint64_t total_packets = 0;
        uint64_t total_drops = 0;
        uint64_t total_bytes = 0;
        uint64_t delta_packets = 0;
        uint64_t delta_drops = 0;
        uint64_t delta_bytes = 0;
    };

    struct ChannelAccumulator {
        uint64_t total_packets = 0;
        uint64_t total_bytes = 0;
        uint64_t delta_packets = 0;
        uint64_t delta_bytes = 0;
    };

    void RunAggregator();
    StatsSnapshot BuildSnapshotViewLocked(double intervalSeconds,
                                         bool preserveLastWindowWhenIdle) const;
    void RefreshCpuUsageLocked();
    void ResetDeltasLocked();
    static double ComputeDropRate(uint64_t deltaPackets, uint64_t deltaDrops);
    static double ComputeP99Locked(const std::deque<double>& latencies);

    static constexpr std::chrono::milliseconds kAggregationInterval{100};
    static constexpr size_t kLatencyWindowSize = 256;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::thread worker_;
    std::chrono::steady_clock::time_point last_flush_at_;
    StatsSnapshot snapshot_;
    CpuUsageSampler cpu_usage_sampler_;
    std::vector<std::function<void(const StatsSnapshot&)>> callbacks_;

    uint64_t total_packets_ = 0;
    uint64_t total_drops_ = 0;
    uint64_t total_bytes_ = 0;
    uint64_t delta_packets_ = 0;
    uint64_t delta_drops_ = 0;
    uint64_t delta_bytes_ = 0;

    std::map<std::string, ProtocolAccumulator> protocol_stats_;
    std::map<uint32_t, ChannelAccumulator> channel_stats_;
    std::deque<double> latency_window_ms_;
};

} // namespace recplay
