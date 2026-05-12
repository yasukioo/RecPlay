// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "ITimelineService.h"

#include <atomic>
#include <chrono>
#include <mutex>

namespace recplay {

class TimelineEngine final : public ITimelineService {
public:
    TimelineEngine();

    void SetOrigin(uint64_t t0_ns) override;
    void SetSpeed(double speed) override;
    double GetSpeed() const override;
    uint64_t Now() const override;
    void SeekTo(uint64_t target_ns) override;
    void Start() override;
    void Pause() override;
    void Resume() override;
    bool IsPaused() const override;

private:
    using Clock = std::chrono::steady_clock;
    uint64_t ComputeNowLocked(Clock::time_point now) const;

    mutable std::mutex mutex_;
    std::atomic<uint64_t> origin_ns_{0};
    std::atomic<double> speed_{1.0};
    std::atomic<bool> paused_{true};
    std::atomic<uint64_t> paused_at_ns_{0};
    std::chrono::steady_clock::time_point clock_start_;
};

} // namespace recplay
