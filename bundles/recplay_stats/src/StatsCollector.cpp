// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StatsCollector.h"

namespace recplay {

StatsSnapshot StatsCollector::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void StatsCollector::OnUpdate(std::function<void(const StatsSnapshot&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

void StatsCollector::RecordPacket(const std::string& protocol, uint32_t channelId, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.total_packets += 1;
    snapshot_.total_throughput_mbps += static_cast<double>(bytes) * 8.0 / 1'000'000.0;
    snapshot_.per_protocol[protocol].packets += 1;
    snapshot_.per_channel[channelId].packets += 1;
    if (callback_) {
        callback_(snapshot_);
    }
}

void StatsCollector::RecordDrop(const std::string& protocol, uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.total_drops += count;
    snapshot_.per_protocol[protocol].drops += count;
}

void StatsCollector::RecordWriteLatency(double ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.write_latency_p99_ms = ms;
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

} // namespace recplay
