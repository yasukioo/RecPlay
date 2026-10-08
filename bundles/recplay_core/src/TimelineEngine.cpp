// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "TimelineEngine.h"

namespace recplay {

TimelineEngine::TimelineEngine()
    : clock_start_ns_(ToSteadyNanoseconds(Clock::now())) {}

void TimelineEngine::SetOrigin(uint64_t t0_ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    origin_ns_.store(t0_ns, std::memory_order_release);
    if (paused_.load(std::memory_order_acquire)) {
        paused_at_ns_.store(t0_ns, std::memory_order_release);
    } else {
        clock_start_ns_.store(ToSteadyNanoseconds(Clock::now()), std::memory_order_release);
    }
}

void TimelineEngine::SetSpeed(double speed) {
    if (speed <= 0.0) {
        speed = 1.0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!paused_.load(std::memory_order_acquire)) {
        const auto now_ns = ToSteadyNanoseconds(Clock::now());
        origin_ns_.store(ComputeNowLocked(now_ns), std::memory_order_release);
        clock_start_ns_.store(now_ns, std::memory_order_release);
    }
    speed_.store(speed, std::memory_order_release);
}

double TimelineEngine::GetSpeed() const {
    return speed_.load(std::memory_order_acquire);
}

uint64_t TimelineEngine::Now() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ComputeNowLocked(ToSteadyNanoseconds(Clock::now()));
}

void TimelineEngine::SeekTo(uint64_t target_ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    origin_ns_.store(target_ns, std::memory_order_release);
    if (paused_.load(std::memory_order_acquire)) {
        paused_at_ns_.store(target_ns, std::memory_order_release);
    } else {
        clock_start_ns_.store(ToSteadyNanoseconds(Clock::now()), std::memory_order_release);
    }
}

void TimelineEngine::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    clock_start_ns_.store(ToSteadyNanoseconds(Clock::now()), std::memory_order_release);
    paused_at_ns_.store(origin_ns_.load(std::memory_order_acquire), std::memory_order_release);
    paused_.store(false, std::memory_order_release);
}

void TimelineEngine::Pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (paused_.load(std::memory_order_acquire)) {
        return;
    }
    paused_at_ns_.store(
        ComputeNowLocked(ToSteadyNanoseconds(Clock::now())),
        std::memory_order_release);
    paused_.store(true, std::memory_order_release);
}

void TimelineEngine::Resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!paused_.load(std::memory_order_acquire)) {
        return;
    }
    const auto paused_at = paused_at_ns_.load(std::memory_order_acquire);
    origin_ns_.store(paused_at, std::memory_order_release);
    clock_start_ns_.store(ToSteadyNanoseconds(Clock::now()), std::memory_order_release);
    paused_.store(false, std::memory_order_release);
}

bool TimelineEngine::IsPaused() const {
    return paused_.load(std::memory_order_acquire);
}

int64_t TimelineEngine::ToSteadyNanoseconds(Clock::time_point time_point) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time_point.time_since_epoch()).count();
}

uint64_t TimelineEngine::ComputeNowLocked(int64_t now_ns) const {
    const auto origin = origin_ns_.load(std::memory_order_acquire);
    if (paused_.load(std::memory_order_acquire)) {
        return paused_at_ns_.load(std::memory_order_acquire);
    }

    const auto start_ns = clock_start_ns_.load(std::memory_order_acquire);
    const auto elapsed = now_ns > start_ns ? now_ns - start_ns : 0;
    const auto scaled = static_cast<uint64_t>(static_cast<double>(elapsed) * GetSpeed());
    return origin + scaled;
}

} // namespace recplay
