// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstdint>

namespace recplay {

class ITimelineService {
public:
    virtual ~ITimelineService() = default;

    virtual void SetOrigin(uint64_t t0_ns) = 0;
    virtual void SetSpeed(double speed) = 0;
    virtual double GetSpeed() const = 0;
    virtual uint64_t Now() const = 0;
    virtual void SeekTo(uint64_t target_ns) = 0;
    virtual void Start() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual bool IsPaused() const = 0;
};

} // namespace recplay
