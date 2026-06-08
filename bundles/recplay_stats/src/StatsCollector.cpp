// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StatsCollector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace recplay {

namespace {

StatsCollector::CpuUsageSampler CreateDefaultCpuUsageSampler() {
#if defined(_WIN32)
    struct State {
        bool has_previous = false;
        uint64_t idle = 0;
        uint64_t total = 0;
    };

    auto state = std::make_shared<State>();
    return [state]() -> std::optional<double> {
        FILETIME idleTime{};
        FILETIME kernelTime{};
        FILETIME userTime{};
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return std::nullopt;
        }

        ULARGE_INTEGER idleValue{};
        idleValue.LowPart = idleTime.dwLowDateTime;
        idleValue.HighPart = idleTime.dwHighDateTime;

        ULARGE_INTEGER kernelValue{};
        kernelValue.LowPart = kernelTime.dwLowDateTime;
        kernelValue.HighPart = kernelTime.dwHighDateTime;

        ULARGE_INTEGER userValue{};
        userValue.LowPart = userTime.dwLowDateTime;
        userValue.HighPart = userTime.dwHighDateTime;

        const uint64_t idle = idleValue.QuadPart;
        const uint64_t total = kernelValue.QuadPart + userValue.QuadPart;
        if (!state->has_previous) {
            state->has_previous = true;
            state->idle = idle;
            state->total = total;
            return std::nullopt;
        }

        const uint64_t idleDelta = idle - state->idle;
        const uint64_t totalDelta = total - state->total;
        state->idle = idle;
        state->total = total;

        if (totalDelta == 0 || totalDelta < idleDelta) {
            return 0.0;
        }

        const double usage =
            static_cast<double>(totalDelta - idleDelta) * 100.0 /
            static_cast<double>(totalDelta);
        return std::clamp(usage, 0.0, 100.0);
    };
#elif defined(__linux__)
    struct State {
        bool has_previous = false;
        uint64_t idle = 0;
        uint64_t total = 0;
    };

    auto state = std::make_shared<State>();
    return [state]() -> std::optional<double> {
        std::ifstream input("/proc/stat");
        if (!input.is_open()) {
            return std::nullopt;
        }

        std::string label;
        uint64_t user = 0;
        uint64_t nice = 0;
        uint64_t system = 0;
        uint64_t idle = 0;
        uint64_t iowait = 0;
        uint64_t irq = 0;
        uint64_t softirq = 0;
        uint64_t steal = 0;
        if (!(input >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) ||
            label != "cpu") {
            return std::nullopt;
        }

        const uint64_t idleAll = idle + iowait;
        const uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
        if (!state->has_previous) {
            state->has_previous = true;
            state->idle = idleAll;
            state->total = total;
            return std::nullopt;
        }

        const uint64_t idleDelta = idleAll - state->idle;
        const uint64_t totalDelta = total - state->total;
        state->idle = idleAll;
        state->total = total;

        if (totalDelta == 0 || totalDelta < idleDelta) {
            return 0.0;
        }

        const double usage =
            static_cast<double>(totalDelta - idleDelta) * 100.0 /
            static_cast<double>(totalDelta);
        return std::clamp(usage, 0.0, 100.0);
    };
#else
    return []() -> std::optional<double> {
        return std::nullopt;
    };
#endif
}

} // namespace

StatsCollector::StatsCollector(CpuUsageSampler cpuUsageSampler)
    : last_flush_at_(std::chrono::steady_clock::now()),
      cpu_usage_sampler_(cpuUsageSampler ? std::move(cpuUsageSampler) : CreateDefaultCpuUsageSampler()) {
    RefreshCpuUsageLocked();
    worker_ = std::thread(&StatsCollector::RunAggregator, this);
}

StatsCollector::~StatsCollector() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

StatsSnapshot StatsCollector::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const_cast<StatsCollector*>(this)->RefreshCpuUsageLocked();
    const auto now = std::chrono::steady_clock::now();
    const double intervalSeconds =
        (std::max)(std::chrono::duration<double>(now - last_flush_at_).count(), 1e-6);
    return BuildSnapshotViewLocked(intervalSeconds, true);
}

void StatsCollector::OnUpdate(std::function<void(const StatsSnapshot&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (callbacks_.size() >= kMaxCallbacks) {
        callbacks_.erase(callbacks_.begin());
    }
    callbacks_.push_back(std::move(cb));
}

void StatsCollector::RecordPacket(const std::string& protocol, uint32_t channelId, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t byteCount = static_cast<uint64_t>(bytes);
    ++total_packets_;
    ++delta_packets_;
    total_bytes_ += byteCount;
    delta_bytes_ += byteCount;

    auto& protocolEntry = protocol_stats_[protocol];
    ++protocolEntry.total_packets;
    ++protocolEntry.delta_packets;
    protocolEntry.total_bytes += byteCount;
    protocolEntry.delta_bytes += byteCount;

    auto& channelEntry = channel_stats_[channelId];
    ++channelEntry.total_packets;
    ++channelEntry.delta_packets;
    channelEntry.total_bytes += byteCount;
    channelEntry.delta_bytes += byteCount;
}

void StatsCollector::RecordDrop(const std::string& protocol, uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_drops_ += count;
    delta_drops_ += count;

    auto& protocolEntry = protocol_stats_[protocol];
    protocolEntry.total_drops += count;
    protocolEntry.delta_drops += count;
}

void StatsCollector::RecordWriteLatency(double ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    latency_window_ms_.push_back(ms);
    while (latency_window_ms_.size() > kLatencyWindowSize) {
        latency_window_ms_.pop_front();
    }
}

void StatsCollector::UpdateRingBufferState(uint32_t used, uint32_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.ringbuf_used = used;
    snapshot_.ringbuf_capacity = capacity;
}

void StatsCollector::UpdateDiskQueue(uint64_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.disk_queue_bytes = bytes;
}

void StatsCollector::RunAggregator() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        const auto wakeAt = last_flush_at_ + kAggregationInterval;
        cv_.wait_until(lock, wakeAt, [this, wakeAt] {
            return stop_ || std::chrono::steady_clock::now() >= wakeAt;
        });
        if (stop_) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - last_flush_at_;
        last_flush_at_ = now;

        const double intervalSeconds =
            (std::max)(std::chrono::duration<double>(elapsed).count(), 1e-6);
        RefreshCpuUsageLocked();
        snapshot_ = BuildSnapshotViewLocked(intervalSeconds, false);
        ResetDeltasLocked();
        const StatsSnapshot currentSnapshot = snapshot_;
        const auto callbacks = callbacks_;

        lock.unlock();
        for (const auto& callback : callbacks) {
            if (callback) {
                callback(currentSnapshot);
            }
        }
        lock.lock();
    }
}

StatsSnapshot StatsCollector::BuildSnapshotViewLocked(double intervalSeconds,
                                                     bool preserveLastWindowWhenIdle) const {
    StatsSnapshot snapshot = snapshot_;
    const bool hasPendingWindow =
        delta_packets_ != 0 || delta_drops_ != 0 || delta_bytes_ != 0;

    snapshot.total_packets = total_packets_;
    snapshot.total_drops = total_drops_;
    snapshot.total_bytes = total_bytes_;
    snapshot.write_latency_p99_ms = ComputeP99Locked(latency_window_ms_);

    if (hasPendingWindow) {
        snapshot.total_throughput_mbps =
            static_cast<double>(delta_bytes_) * 8.0 / 1'000'000.0 / intervalSeconds;
        snapshot.drop_rate = ComputeDropRate(delta_packets_, delta_drops_);

        snapshot.per_protocol.clear();
        for (const auto& [protocol, stats] : protocol_stats_) {
            ProtocolStats protocolSnapshot;
            protocolSnapshot.packets = stats.total_packets;
            protocolSnapshot.drops = stats.total_drops;
            protocolSnapshot.throughput_mbps =
                static_cast<double>(stats.delta_bytes) * 8.0 / 1'000'000.0 / intervalSeconds;
            snapshot.per_protocol.emplace(protocol, protocolSnapshot);
        }

        snapshot.per_channel.clear();
        for (const auto& [channelId, stats] : channel_stats_) {
            ChannelStats channelSnapshot;
            channelSnapshot.packets = stats.total_packets;
            channelSnapshot.throughput_mbps =
                static_cast<double>(stats.delta_bytes) * 8.0 / 1'000'000.0 / intervalSeconds;
            snapshot.per_channel.emplace(channelId, channelSnapshot);
        }
    } else if (!preserveLastWindowWhenIdle) {
        snapshot.total_throughput_mbps = 0.0;
        snapshot.drop_rate = 0.0;

        for (auto& [_, protocolSnapshot] : snapshot.per_protocol) {
            protocolSnapshot.throughput_mbps = 0.0;
        }
        for (auto& [_, channelSnapshot] : snapshot.per_channel) {
            channelSnapshot.throughput_mbps = 0.0;
        }
    }

    return snapshot;
}

void StatsCollector::RefreshCpuUsageLocked() {
    if (!cpu_usage_sampler_) {
        snapshot_.cpu_usage_percent = std::nullopt;
        return;
    }

    snapshot_.cpu_usage_percent = cpu_usage_sampler_();
}

void StatsCollector::ResetDeltasLocked() {
    delta_packets_ = 0;
    delta_drops_ = 0;
    delta_bytes_ = 0;
    for (auto& [_, stats] : protocol_stats_) {
        stats.delta_packets = 0;
        stats.delta_drops = 0;
        stats.delta_bytes = 0;
    }
    for (auto& [_, stats] : channel_stats_) {
        stats.delta_packets = 0;
        stats.delta_bytes = 0;
    }
}

double StatsCollector::ComputeDropRate(uint64_t deltaPackets, uint64_t deltaDrops) {
    const uint64_t total = deltaPackets + deltaDrops;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(deltaDrops) / static_cast<double>(total);
}

double StatsCollector::ComputeP99Locked(const std::deque<double>& latencies) {
    if (latencies.empty()) {
        return 0.0;
    }

    std::vector<double> sorted(latencies.begin(), latencies.end());
    std::sort(sorted.begin(), sorted.end());

    const double rank = std::ceil(0.99 * static_cast<double>(sorted.size()));
    const size_t index = static_cast<size_t>((std::max)(1.0, rank)) - 1;
    return sorted[(std::min)(index, sorted.size() - 1)];
}

} // namespace recplay
