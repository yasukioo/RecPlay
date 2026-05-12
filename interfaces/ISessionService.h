// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <functional>
#include <string>

namespace recplay {

using StateCallback = std::function<void(SessionState oldState, SessionState newState)>;

class ISessionService {
public:
    virtual ~ISessionService() = default;

    virtual SessionState GetState() const = 0;
    virtual void OnStateChanged(StateCallback cb) = 0;

    virtual bool StartRecording(const std::string& configJson) = 0;
    virtual void PauseRecording() = 0;
    virtual void ResumeRecording() = 0;
    virtual void StopRecording() = 0;

    virtual bool OpenForPlayback(const std::string& filePath) = 0;
    virtual void Play(double speed = 1.0) = 0;
    virtual void Pause() = 0;
    virtual void SeekTo(uint64_t timestamp_ns) = 0;
    virtual void SetSpeed(double speed) = 0;
    virtual void SetLoopRange(uint64_t startNs, uint64_t endNs) = 0;
    virtual void Stop() = 0;

    virtual uint64_t GetDuration() const = 0;
    virtual uint64_t GetCurrentPosition() const = 0;
    virtual double GetCurrentSpeed() const = 0;
};

} // namespace recplay
