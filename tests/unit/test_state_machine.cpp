// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "SessionStateMachine.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using recplay::SessionState;
using recplay::SessionStateMachine;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestValidTransitionsCoverRecordingAndPlaybackFlow() {
    SessionStateMachine machine;

    Expect(machine.Transition(SessionState::Recording), "Idle should transition to Recording");
    Expect(machine.Transition(SessionState::RecordingPaused), "Recording should transition to RecordingPaused");
    Expect(machine.Transition(SessionState::Recording), "RecordingPaused should transition back to Recording");
    Expect(machine.Transition(SessionState::Stopped), "Recording should transition to Stopped");
    Expect(machine.Transition(SessionState::Playing), "Stopped should transition to Playing");
    Expect(machine.Transition(SessionState::Seeking), "Playing should transition to Seeking");
    Expect(machine.Transition(SessionState::PlaybackPaused), "Seeking should transition to PlaybackPaused");
    Expect(machine.Transition(SessionState::Playing), "PlaybackPaused should transition back to Playing");
    Expect(machine.Transition(SessionState::Stopped), "Playing should transition to Stopped");
    Expect(machine.Transition(SessionState::Idle), "Stopped should transition back to Idle");
    Expect(machine.GetState() == SessionState::Idle, "full valid flow should end at Idle");
}

void TestInvalidTransitionsAreRejectedWithoutStateChanges() {
    SessionStateMachine machine;

    Expect(!machine.Transition(SessionState::Playing), "Idle should reject direct transition to Playing");
    Expect(machine.GetState() == SessionState::Idle, "invalid transition should not change state");

    Expect(machine.Transition(SessionState::Stopped), "Idle should allow transition to Stopped");
    Expect(!machine.Transition(SessionState::RecordingPaused),
           "Stopped should reject direct transition to RecordingPaused");
    Expect(machine.GetState() == SessionState::Stopped,
           "invalid transition from Stopped should keep state unchanged");
}

void TestCallbacksObserveSuccessfulTransitionsInOrder() {
    SessionStateMachine machine;
    std::vector<std::pair<SessionState, SessionState>> callback_one_events;
    std::vector<std::pair<SessionState, SessionState>> callback_two_events;

    machine.OnTransition([&](SessionState from, SessionState to) {
        callback_one_events.emplace_back(from, to);
    });
    machine.OnTransition([&](SessionState from, SessionState to) {
        callback_two_events.emplace_back(from, to);
    });

    Expect(machine.Transition(SessionState::Stopped), "Idle should transition to Stopped for callback test");
    Expect(machine.Transition(SessionState::Playing), "Stopped should transition to Playing for callback test");

    const std::vector<std::pair<SessionState, SessionState>> expected = {
        {SessionState::Idle, SessionState::Stopped},
        {SessionState::Stopped, SessionState::Playing},
    };
    Expect(callback_one_events == expected, "first callback should receive every successful transition");
    Expect(callback_two_events == expected, "second callback should receive every successful transition");
}

} // namespace

int main() {
    try {
        TestValidTransitionsCoverRecordingAndPlaybackFlow();
        TestInvalidTransitionsAreRejectedWithoutStateChanges();
        TestCallbacksObserveSuccessfulTransitionsInOrder();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
