// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "SessionController.h"

#include "CoreEngine.h"

namespace recplay {

SessionController::SessionController(CoreEngine* engine) : engine_(engine) {
    machine_.OnTransition([this](SessionState from, SessionState to) {
        PublishState(from, to);
    });
}

SessionState SessionController::GetState() const {
    return machine_.GetState();
}

void SessionController::OnStateChanged(StateCallback cb) {
    state_cb_ = std::move(cb);
}

bool SessionController::StartRecording(const std::string& configJson) {
    (void)configJson;
    if (!machine_.Transition(SessionState::Recording)) {
        return false;
    }
    if (engine_) {
        engine_->Timeline().Start();
    }
    return true;
}

void SessionController::PauseRecording() {
    machine_.Transition(SessionState::RecordingPaused);
}

void SessionController::ResumeRecording() {
    machine_.Transition(SessionState::Recording);
}

void SessionController::StopRecording() {
    machine_.Transition(SessionState::Stopped);
}

bool SessionController::OpenForPlayback(const std::string& filePath) {
    (void)filePath;
    return machine_.Transition(SessionState::Stopped);
}

void SessionController::Play(double speed) {
    current_speed_ = speed;
    if (engine_) {
        engine_->Timeline().SetSpeed(speed);
        engine_->Timeline().Start();
        engine_->Scheduler().Start([](PacketPtr) {});
    }
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
    current_position_ns_ = timestamp_ns;
    if (engine_) {
        engine_->Timeline().SeekTo(timestamp_ns);
    }
    machine_.Transition(SessionState::Seeking);
    machine_.Transition(SessionState::Playing);
}

void SessionController::SetSpeed(double speed) {
    current_speed_ = speed;
    if (engine_) {
        engine_->Timeline().SetSpeed(speed);
    }
}

void SessionController::SetLoopRange(uint64_t startNs, uint64_t endNs) {
    loop_start_ns_ = startNs;
    loop_end_ns_ = endNs;
}

void SessionController::Stop() {
    if (engine_) {
        engine_->Scheduler().Stop();
    }
    machine_.Transition(SessionState::Stopped);
}

uint64_t SessionController::GetDuration() const {
    return duration_ns_;
}

uint64_t SessionController::GetCurrentPosition() const {
    return current_position_ns_;
}

double SessionController::GetCurrentSpeed() const {
    return current_speed_;
}

void SessionController::PublishState(SessionState from, SessionState next) {
    if (state_cb_) {
        state_cb_(from, next);
    }
}

} // namespace recplay
