// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StatsCollector.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using recplay::StatsCollector;
using recplay::StatsSnapshot;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqual(double lhs, double rhs, double tolerance) {
    return std::fabs(lhs - rhs) <= tolerance;
}

StatsSnapshot WaitForSnapshot(StatsCollector& collector,
                              const std::atomic<uint64_t>& updates,
                              uint64_t previousUpdates,
                              std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (updates.load() > previousUpdates) {
            return collector.GetSnapshot();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    throw std::runtime_error("Timed out waiting for stats update");
}

void TestAggregatesPeriodically() {
    StatsCollector collector;
    std::atomic<uint64_t> updatesA{0};
    std::atomic<uint64_t> updatesB{0};

    collector.OnUpdate([&](const StatsSnapshot&) { updatesA.fetch_add(1); });
    collector.OnUpdate([&](const StatsSnapshot&) { updatesB.fetch_add(1); });

    const uint64_t before = updatesA.load();
    collector.RecordPacket("UDP", 7, 125000);
    collector.RecordPacket("UDP", 7, 125000);
    collector.RecordDrop("UDP", 1);
    collector.RecordWriteLatency(1.0);
    collector.RecordWriteLatency(2.0);
    collector.RecordWriteLatency(3.0);
    collector.UpdateRingBufferState(5, 64);
    collector.UpdateDiskQueue(4096);

    const StatsSnapshot snapshot =
        WaitForSnapshot(collector, updatesA, before, std::chrono::milliseconds(500));

    Expect(updatesA.load() >= 1, "primary callback should be invoked by aggregator");
    Expect(updatesB.load() >= 1, "secondary callback should be invoked by aggregator");
    Expect(snapshot.total_packets == 2, "expected total packet count to aggregate");
    Expect(snapshot.total_drops == 1, "expected total drop count to aggregate");
    Expect(snapshot.per_protocol.at("UDP").packets == 2, "expected per-protocol packet count");
    Expect(snapshot.per_protocol.at("UDP").drops == 1, "expected per-protocol drop count");
    Expect(snapshot.per_channel.at(7).packets == 2, "expected per-channel packet count");
    Expect(snapshot.ringbuf_used == 5 && snapshot.ringbuf_capacity == 64, "expected ring buffer state");
    Expect(snapshot.disk_queue_bytes == 4096, "expected disk queue bytes");
    Expect(snapshot.total_throughput_mbps > 0.0, "expected positive throughput");
    Expect(snapshot.per_protocol.at("UDP").throughput_mbps > 0.0, "expected protocol throughput");
    Expect(snapshot.per_channel.at(7).throughput_mbps > 0.0, "expected channel throughput");
    Expect(NearlyEqual(snapshot.write_latency_p99_ms, 3.0, 0.25), "expected p99 from latency window");
    Expect(snapshot.drop_rate > 0.0, "expected non-zero drop rate");
}

void TestThroughputResetsBetweenWindows() {
    StatsCollector collector;
    std::atomic<uint64_t> updates{0};

    collector.OnUpdate([&](const StatsSnapshot&) { updates.fetch_add(1); });

    const uint64_t before = updates.load();
    collector.RecordPacket("TCP", 3, 250000);
    const StatsSnapshot active =
        WaitForSnapshot(collector, updates, before, std::chrono::milliseconds(500));
    Expect(active.total_throughput_mbps > 0.0, "expected throughput during active window");

    const uint64_t afterActive = updates.load();
    const StatsSnapshot idle =
        WaitForSnapshot(collector, updates, afterActive, std::chrono::milliseconds(500));
    Expect(NearlyEqual(idle.total_throughput_mbps, 0.0, 0.05),
           "expected throughput to reset after idle window: " +
               std::to_string(idle.total_throughput_mbps));
    Expect(NearlyEqual(idle.per_protocol.at("TCP").throughput_mbps, 0.0, 0.05),
           "expected protocol throughput to reset: " +
               std::to_string(idle.per_protocol.at("TCP").throughput_mbps));
    Expect(NearlyEqual(idle.per_channel.at(3).throughput_mbps, 0.0, 0.05),
           "expected channel throughput to reset: " +
               std::to_string(idle.per_channel.at(3).throughput_mbps));
}

void TestGetSnapshotIncludesPendingStatsBeforeFlush() {
    StatsCollector collector;

    collector.RecordPacket("UDP", 9, 4096);
    collector.RecordDrop("UDP", 2);
    collector.RecordWriteLatency(4.0);
    collector.UpdateRingBufferState(3, 32);
    collector.UpdateDiskQueue(2048);

    const StatsSnapshot snapshot = collector.GetSnapshot();

    Expect(snapshot.total_packets == 1, "expected immediate total packet count");
    Expect(snapshot.total_drops == 2, "expected immediate total drop count");
    Expect(snapshot.per_protocol.at("UDP").packets == 1, "expected immediate per-protocol packet count");
    Expect(snapshot.per_protocol.at("UDP").drops == 2, "expected immediate per-protocol drop count");
    Expect(snapshot.per_channel.at(9).packets == 1, "expected immediate per-channel packet count");
    Expect(snapshot.write_latency_p99_ms == 4.0, "expected immediate latency p99");
    Expect(snapshot.ringbuf_used == 3 && snapshot.ringbuf_capacity == 32, "expected immediate ring buffer state");
    Expect(snapshot.disk_queue_bytes == 2048, "expected immediate disk queue bytes");
}

} // namespace

int main() {
    try {
        TestAggregatesPeriodically();
        TestThroughputResetsBetweenWindows();
        TestGetSnapshotIncludesPendingStatsBeforeFlush();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
