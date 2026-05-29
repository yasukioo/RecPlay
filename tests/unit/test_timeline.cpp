// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "TimelineEngine.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using recplay::TimelineEngine;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void SleepFor(std::chrono::milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

void TestPausedTimelineTracksOriginAndSeek() {
    TimelineEngine timeline;

    Expect(timeline.IsPaused(), "timeline should start paused");
    Expect(timeline.Now() == 0, "paused timeline should start at zero");

    timeline.SetOrigin(1'000);
    Expect(timeline.Now() == 1'000, "setting origin while paused should update Now()");

    timeline.SeekTo(250'000'000ULL);
    Expect(timeline.Now() == 250'000'000ULL, "seek while paused should jump immediately");
}

void TestPauseAndResumeFreezeAndRestartProgress() {
    TimelineEngine timeline;
    timeline.SetOrigin(0);
    timeline.Start();
    SleepFor(std::chrono::milliseconds(30));

    const auto running_now = timeline.Now();
    Expect(running_now >= 10'000'000ULL, "running timeline should advance with wall time");

    timeline.Pause();
    const auto paused_now = timeline.Now();
    SleepFor(std::chrono::milliseconds(20));
    Expect(timeline.Now() == paused_now, "paused timeline should stop advancing");

    timeline.SeekTo(500'000'000ULL);
    Expect(timeline.Now() == 500'000'000ULL, "seek while paused should update paused position");

    timeline.Resume();
    SleepFor(std::chrono::milliseconds(20));
    Expect(timeline.Now() > 500'000'000ULL, "resume should continue advancing from paused position");
}

void TestSpeedScalingAndInvalidSpeedFallback() {
    TimelineEngine timeline;
    timeline.SetOrigin(0);
    timeline.Start();
    SleepFor(std::chrono::milliseconds(40));
    const auto baseline = timeline.Now();

    timeline.SetSpeed(2.0);
    SleepFor(std::chrono::milliseconds(40));
    const auto accelerated = timeline.Now();
    const auto accelerated_delta = accelerated - baseline;
    Expect(accelerated_delta >= 50'000'000ULL,
           "2x speed should advance notably faster than wall time, actual delta=" +
               std::to_string(accelerated_delta));

    timeline.Pause();
    timeline.SetSpeed(0.0);
    Expect(timeline.GetSpeed() == 1.0, "non-positive playback speed should fall back to 1.0x");
}

} // namespace

int main() {
    try {
        TestPausedTimelineTracksOriginAndSeek();
        TestPauseAndResumeFreezeAndRestartProgress();
        TestSpeedScalingAndInvalidSpeedFallback();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
