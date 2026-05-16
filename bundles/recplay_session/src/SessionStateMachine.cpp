// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "SessionStateMachine.h"

namespace recplay {

SessionState SessionStateMachine::GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool SessionStateMachine::IsValidTransition(SessionState from, SessionState to) const {
    switch (from) {
        case SessionState::Idle:
            return to == SessionState::Recording || to == SessionState::Stopped;
        case SessionState::Recording:
            return to == SessionState::RecordingPaused || to == SessionState::Stopped;
        case SessionState::RecordingPaused:
            return to == SessionState::Recording || to == SessionState::Stopped;
        case SessionState::Stopped:
            return to == SessionState::Playing || to == SessionState::Idle;
        case SessionState::Playing:
            return to == SessionState::PlaybackPaused || to == SessionState::Seeking || to == SessionState::Stopped;
        case SessionState::PlaybackPaused:
            return to == SessionState::Playing || to == SessionState::Seeking || to == SessionState::Stopped;
        case SessionState::Seeking:
            return to == SessionState::Playing || to == SessionState::PlaybackPaused;
    }
    return false;
}

bool SessionStateMachine::Transition(SessionState target) {
    std::vector<TransitionCallback> callbacks;
    SessionState from;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        from = state_;
        if (!IsValidTransition(from, target)) {
            return false;
        }
        state_ = target;
        callbacks = callbacks_;
    }

    for (const auto& callback : callbacks) {
        if (callback) {
            callback(from, target);
        }
    }
    return true;
}

void SessionStateMachine::OnTransition(TransitionCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(cb));
}

} // namespace recplay
