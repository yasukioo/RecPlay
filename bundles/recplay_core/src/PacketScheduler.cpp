// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "PacketScheduler.h"

#include <chrono>

namespace recplay {

PacketScheduler::PacketScheduler(TimelineEngine& timeline) : timeline_(timeline) {}

void PacketScheduler::Enqueue(PacketPtr pkt) {
    if (!pkt) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(TimedPacket{ComputeDispatchTime(pkt->t_capture), std::move(pkt)});
    cv_.notify_all();
}

void PacketScheduler::Start(std::function<void(PacketPtr)> dispatchFn) {
    Stop();
    dispatch_fn_ = std::move(dispatchFn);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&PacketScheduler::SchedulerLoop, this);
}

void PacketScheduler::Stop() {
    running_.store(false, std::memory_order_release);
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void PacketScheduler::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_ = decltype(queue_)();
    cv_.notify_all();
}

void PacketScheduler::SchedulerLoop() {
    while (running_.load(std::memory_order_acquire)) {
        PacketPtr pkt;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (running_.load(std::memory_order_acquire)) {
                if (queue_.empty()) {
                    cv_.wait(lock, [&] {
                        return !running_.load(std::memory_order_acquire) || !queue_.empty();
                    });
                    continue;
                }

                const auto next_dispatch = queue_.top().dispatch_time;
                const auto now = Clock::now();
                if (next_dispatch <= now) {
                    pkt = queue_.top().packet;
                    queue_.pop();
                    break;
                }

                cv_.wait_until(lock, next_dispatch, [&] {
                    return !running_.load(std::memory_order_acquire);
                });
            }

            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
        }

        if (pkt && dispatch_fn_) {
            dispatch_fn_(std::move(pkt));
        }
    }
}

PacketScheduler::Clock::time_point PacketScheduler::ComputeDispatchTime(
    uint64_t capture_time_ns) const {
    const auto timeline_now_ns = timeline_.Now();
    const auto now = Clock::now();
    if (capture_time_ns <= timeline_now_ns) {
        return now;
    }

    return now + std::chrono::nanoseconds(capture_time_ns - timeline_now_ns);
}

} // namespace recplay
