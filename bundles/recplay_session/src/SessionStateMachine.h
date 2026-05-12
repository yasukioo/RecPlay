// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <functional>
#include <mutex>
#include <vector>

namespace recplay {

class SessionStateMachine {
public:
    using TransitionCallback = std::function<void(SessionState from, SessionState to)>;

    SessionState GetState() const;
    bool Transition(SessionState target);
    void OnTransition(TransitionCallback cb);

private:
    bool IsValidTransition(SessionState from, SessionState to) const;

    mutable std::mutex mutex_;
    SessionState state_ = SessionState::Idle;
    std::vector<TransitionCallback> callbacks_;
};

} // namespace recplay
