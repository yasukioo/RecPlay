// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "ISessionService.h"
#include "SessionStateMachine.h"

#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace recplay {

class CoreEngine;

class SessionController final : public ISessionService {
public:
    explicit SessionController(CoreEngine* engine = nullptr);

    SessionState GetState() const override;
    void OnStateChanged(StateCallback cb) override;
    bool StartRecording(const std::string& configJson) override;
    void PauseRecording() override;
    void ResumeRecording() override;
    void StopRecording() override;
    bool OpenForPlayback(const std::string& filePath,
                         const std::string& replayConfigJson = "{}") override;
    void Play(double speed = 1.0) override;
    void Pause() override;
    void SeekTo(uint64_t timestamp_ns) override;
    void SetSpeed(double speed) override;
    void SetLoopRange(uint64_t startNs, uint64_t endNs) override;
    void Stop() override;
    void Reset() override;
    uint64_t GetDuration() const override;
    uint64_t GetCurrentPosition() const override;
    double GetCurrentSpeed() const override;
    PlaybackPacketSnapshot GetCurrentPlaybackPacket() const override;
    std::vector<ReplayTarget> GetReplayTargets() const override;
    void SetReplayTargets(const std::vector<ReplayTarget>& targets) override;

private:
    void DispatchPlaybackPacket(PacketPtr pkt);
    void StopRecordingProtocols();
    void StopReplayProtocols();
    void RefreshReplayTargetStatusesLocked(const std::string* failingProtocol = nullptr);
    bool FillPlaybackQueue();
    void PublishState(SessionState from, SessionState next);

    static constexpr size_t kPlaybackPrefetchPackets = 3;

    CoreEngine* engine_ = nullptr;
    mutable std::mutex mutex_;
    SessionStateMachine machine_;
    std::vector<StateCallback> state_callbacks_;
    uint64_t duration_ns_ = 0;
    uint64_t current_position_ns_ = 0;
    std::atomic<double> current_speed_{1.0};
    uint64_t loop_start_ns_ = 0;
    uint64_t loop_end_ns_ = 0;
    std::vector<std::string> active_record_protocols_;
    std::vector<std::string> active_replay_protocols_;
    std::vector<ReplayTarget> replay_targets_;
    std::string current_record_path_;
    std::string current_playback_file_;
    PlaybackPacketSnapshot current_playback_packet_;
};

} // namespace recplay
