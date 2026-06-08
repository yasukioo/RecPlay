// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace recplay {

using StateCallback = std::function<void(SessionState oldState, SessionState newState)>;

class ISessionService {
public:
    virtual ~ISessionService() = default;

    struct PlaybackPacketSnapshot {
        bool available = false;
        Packet packet;
        uint64_t replay_timestamp_ns = 0;
        std::string writer;
    };

    struct ReplayTarget {
        std::string id;
        std::string name;
        std::string protocol;
        bool enabled = true;
        std::string status = "idle";
        std::map<std::string, std::string> config;
    };

    virtual SessionState GetState() const = 0;
    virtual void OnStateChanged(StateCallback cb) = 0;

    virtual bool StartRecording(const std::string& configJson) = 0;
    virtual void PauseRecording() = 0;
    virtual void ResumeRecording() = 0;
    virtual void StopRecording() = 0;

    virtual bool OpenForPlayback(const std::string& filePath,
                                 const std::string& replayConfigJson = "{}") = 0;
    virtual void Play(double speed = 1.0) = 0;
    virtual void Pause() = 0;
    virtual void SeekTo(uint64_t timestamp_ns) = 0;
    virtual void SetSpeed(double speed) = 0;
    virtual void SetLoopRange(uint64_t startNs, uint64_t endNs) = 0;
    virtual void Stop() = 0;
    // Transition from Stopped (or any non-active state) back to Idle so a new
    // recording can be started without restarting the process.
    virtual void Reset() = 0;

    virtual uint64_t GetDuration() const = 0;
    virtual uint64_t GetCurrentPosition() const = 0;
    virtual double GetCurrentSpeed() const = 0;
    virtual PlaybackPacketSnapshot GetCurrentPlaybackPacket() const = 0;
    virtual std::vector<ReplayTarget> GetReplayTargets() const = 0;
    virtual void SetReplayTargets(const std::vector<ReplayTarget>& targets) = 0;
};

} // namespace recplay
