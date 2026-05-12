// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"
#include "TimelineEngine.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace recplay {

class PacketScheduler {
public:
    explicit PacketScheduler(TimelineEngine& timeline);

    void Enqueue(PacketPtr pkt);
    void Start(std::function<void(PacketPtr)> dispatchFn);
    void Stop();
    void Clear();

private:
    using Clock = std::chrono::steady_clock;

    struct TimedPacket {
        Clock::time_point dispatch_time{};
        PacketPtr packet;

        bool operator>(const TimedPacket& other) const {
            return dispatch_time > other.dispatch_time;
        }
    };

    Clock::time_point ComputeDispatchTime(uint64_t capture_time_ns) const;
    void SchedulerLoop();

    TimelineEngine& timeline_;
    std::priority_queue<TimedPacket, std::vector<TimedPacket>, std::greater<TimedPacket>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::function<void(PacketPtr)> dispatch_fn_;
};

} // namespace recplay
