// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "SessionController.h"

#include "CoreEngine.h"
#include "IStorageService.h"

#include <algorithm>
#include <utility>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

namespace recplay {

namespace {

struct RecordRequest {
    std::string output_path;
    std::string codec = "zstd";
    std::vector<std::string> protocols;
    std::string protocol_config = "{}";
};

std::string ToJsonString(const nlohmann::json& value) {
#if RECPLAY_HAS_JSON
    return value.dump();
#else
    (void)value;
    return "{}";
#endif
}

RecordRequest ParseRecordRequest(const std::string& config_json,
                                 const std::vector<std::string>& available_protocols) {
    RecordRequest request;
#if RECPLAY_HAS_JSON
    const auto document = nlohmann::json::parse(config_json.empty() ? "{}" : config_json);
    request.output_path = document.value("output_path", std::string{});
    request.codec = document.value("codec", std::string("zstd"));

    if (document.contains("protocols") && document.at("protocols").is_array()) {
        request.protocols = document.at("protocols").get<std::vector<std::string>>();
    }
    if (request.protocols.empty()) {
        request.protocols = available_protocols;
    }

    if (document.contains("protocol_config")) {
        request.protocol_config = document.at("protocol_config").dump();
    } else if (document.contains("protocol")) {
        request.protocol_config = document.at("protocol").dump();
    }
#else
    (void)config_json;
    request.protocols = available_protocols;
#endif
    return request;
}

} // namespace

SessionController::SessionController(CoreEngine* engine) : engine_(engine) {
    machine_.OnTransition([this](SessionState from, SessionState to) {
        PublishState(from, to);
    });
}

SessionState SessionController::GetState() const {
    return machine_.GetState();
}

void SessionController::OnStateChanged(StateCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_cb_ = std::move(cb);
}

bool SessionController::StartRecording(const std::string& configJson) {
    if (engine_ == nullptr) {
        return false;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr) {
        return false;
    }

    const auto available_protocols = engine_->GetAttachedProtocolNames();
    const auto request = ParseRecordRequest(configJson, available_protocols);
    if (request.output_path.empty() || request.protocols.empty()) {
        return false;
    }

    std::vector<ChannelInfo> channels;
    for (const auto& protocol_name : request.protocols) {
        auto protocol_channels = engine_->GetChannelsForProtocol(protocol_name);
        channels.insert(channels.end(), protocol_channels.begin(), protocol_channels.end());
    }

    if (!storage->CreateFile(request.output_path, channels, request.codec)) {
        return false;
    }

    std::vector<std::string> started_protocols;
    for (const auto& protocol_name : request.protocols) {
        if (!engine_->StartProtocolCapture(protocol_name, request.protocol_config)) {
            for (const auto& started_name : started_protocols) {
                engine_->StopProtocolCapture(started_name);
            }
            storage->FinalizeFile();
            return false;
        }
        started_protocols.push_back(protocol_name);
    }

    if (!machine_.Transition(SessionState::Recording)) {
        for (const auto& started_name : started_protocols) {
            engine_->StopProtocolCapture(started_name);
        }
        storage->FinalizeFile();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_record_protocols_ = std::move(started_protocols);
        current_record_path_ = request.output_path;
        current_position_ns_ = 0;
        duration_ns_ = 0;
    }
    engine_->Timeline().SetOrigin(0);
    engine_->Timeline().Start();
    return true;
}

void SessionController::PauseRecording() {
    if (!machine_.Transition(SessionState::RecordingPaused)) {
        return;
    }
    if (engine_ != nullptr) {
        engine_->Timeline().Pause();
    }
}

void SessionController::ResumeRecording() {
    if (!machine_.Transition(SessionState::Recording)) {
        return;
    }
    if (engine_ != nullptr) {
        engine_->Timeline().Resume();
    }
}

void SessionController::StopRecording() {
    if (engine_ != nullptr) {
        StopRecordingProtocols();
        if (auto* storage = engine_->Storage(); storage != nullptr && storage->IsWriting()) {
            storage->FinalizeFile();
        }
    }
    machine_.Transition(SessionState::Stopped);
}

bool SessionController::OpenForPlayback(const std::string& filePath) {
    if (engine_ == nullptr) {
        return false;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->OpenFile(filePath)) {
        return false;
    }

    const auto protocol_names = engine_->GetAttachedProtocolNames();
    std::vector<std::string> replay_protocols;
    for (const auto& protocol_name : protocol_names) {
        if (!engine_->StartProtocolReplay(protocol_name, "{}")) {
            for (const auto& started_name : replay_protocols) {
                engine_->StopProtocolReplay(started_name);
            }
            storage->CloseFile();
            return false;
        }
        replay_protocols.push_back(protocol_name);
    }

    const auto header = storage->GetHeader();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_playback_file_ = filePath;
        active_replay_protocols_ = std::move(replay_protocols);
        duration_ns_ = header.duration_ns;
        current_position_ns_ = 0;
    }

    engine_->Timeline().SetOrigin(0);
    engine_->Scheduler().Clear();
    if (!FillPlaybackQueue()) {
        StopReplayProtocols();
        storage->CloseFile();
        return false;
    }

    return machine_.Transition(SessionState::Stopped);
}

void SessionController::Play(double speed) {
    if (engine_ == nullptr) {
        return;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return;
    }

    current_speed_ = speed;
    engine_->Timeline().SetSpeed(speed);
    if (engine_->Timeline().IsPaused()) {
        engine_->Timeline().Resume();
    } else {
        engine_->Timeline().Start();
    }
    engine_->Scheduler().Start([this](PacketPtr pkt) {
        if (engine_ == nullptr || !pkt) {
            return;
        }
        engine_->DispatchPacketToReplayingProtocols(pkt);
        std::lock_guard<std::mutex> lock(mutex_);
        current_position_ns_ = pkt->t_capture;
    });
    machine_.Transition(SessionState::Playing);
}

void SessionController::Pause() {
    if (engine_) {
        engine_->Timeline().Pause();
        engine_->Scheduler().Stop();
    }
    machine_.Transition(SessionState::PlaybackPaused);
}

void SessionController::SeekTo(uint64_t timestamp_ns) {
    if (engine_ == nullptr) {
        return;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return;
    }

    const auto previous_state = machine_.GetState();
    if (!machine_.Transition(SessionState::Seeking)) {
        return;
    }

    engine_->Scheduler().Stop();
    engine_->Scheduler().Clear();
    if (!storage->SeekTo(timestamp_ns)) {
        if (previous_state == SessionState::PlaybackPaused) {
            machine_.Transition(SessionState::PlaybackPaused);
        } else {
            machine_.Transition(SessionState::Playing);
        }
        return;
    }

    engine_->Timeline().SeekTo(timestamp_ns);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_position_ns_ = timestamp_ns;
    }

    FillPlaybackQueue();

    if (previous_state == SessionState::PlaybackPaused) {
        machine_.Transition(SessionState::PlaybackPaused);
    } else {
        Play(current_speed_);
    }
}

void SessionController::SetSpeed(double speed) {
    current_speed_ = speed;
    if (engine_) {
        engine_->Timeline().SetSpeed(speed);
    }
}

void SessionController::SetLoopRange(uint64_t startNs, uint64_t endNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    loop_start_ns_ = startNs;
    loop_end_ns_ = endNs;
}

void SessionController::Stop() {
    if (engine_ != nullptr) {
        engine_->Scheduler().Stop();
        engine_->Scheduler().Clear();
        StopReplayProtocols();
        if (auto* storage = engine_->Storage(); storage != nullptr && storage->IsReading()) {
            storage->CloseFile();
        }
    }
    machine_.Transition(SessionState::Stopped);
}

uint64_t SessionController::GetDuration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return duration_ns_;
}

uint64_t SessionController::GetCurrentPosition() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_position_ns_;
}

double SessionController::GetCurrentSpeed() const {
    return current_speed_;
}

void SessionController::StopRecordingProtocols() {
    if (engine_ == nullptr) {
        return;
    }

    std::vector<std::string> protocols;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        protocols = active_record_protocols_;
        active_record_protocols_.clear();
    }

    for (const auto& protocol_name : protocols) {
        engine_->StopProtocolCapture(protocol_name);
    }
}

void SessionController::StopReplayProtocols() {
    if (engine_ == nullptr) {
        return;
    }

    std::vector<std::string> protocols;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        protocols = active_replay_protocols_;
        active_replay_protocols_.clear();
    }

    for (const auto& protocol_name : protocols) {
        engine_->StopProtocolReplay(protocol_name);
    }
}

bool SessionController::FillPlaybackQueue() {
    if (engine_ == nullptr) {
        return false;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return false;
    }

    bool queued_any = false;
    while (storage->HasMore()) {
        auto pkt = storage->ReadNext();
        if (!pkt) {
            break;
        }
        engine_->Scheduler().Enqueue(pkt);
        queued_any = true;
    }
    return queued_any || !storage->HasMore();
}

void SessionController::PublishState(SessionState from, SessionState next) {
    StateCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = state_cb_;
    }
    if (callback) {
        callback(from, next);
    }
}

} // namespace recplay
