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
    queue_.push(TimedPacket{pkt->t_capture, std::move(pkt)});
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

void PacketScheduler::SchedulerLoop() {
    while (running_.load(std::memory_order_acquire)) {
        PacketPtr pkt;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(10), [&] {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });
            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
            if (queue_.empty()) {
                continue;
            }
            if (queue_.top().dispatch_time > timeline_.Now()) {
                continue;
            }
            pkt = queue_.top().packet;
            queue_.pop();
        }

        if (pkt && dispatch_fn_) {
            dispatch_fn_(std::move(pkt));
        }
    }
}

} // namespace recplay
