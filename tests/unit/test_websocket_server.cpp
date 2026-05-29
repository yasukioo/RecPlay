// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "WebSocketServer.h"
#include "ISessionService.h"
#include "IStatsService.h"

#include <cstddef>
#include <limits>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using recplay::IStatsService;
using recplay::ISessionService;
using recplay::SessionState;
using recplay::StateCallback;
using recplay::StatsSnapshot;
using recplay::WebSocketServer;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeSessionService final : public ISessionService {
public:
    SessionState GetState() const override { return SessionState::Idle; }
    void OnStateChanged(StateCallback cb) override { callback = std::move(cb); }
    bool StartRecording(const std::string&) override { return false; }
    void PauseRecording() override {}
    void ResumeRecording() override {}
    void StopRecording() override {}
    bool OpenForPlayback(const std::string&, const std::string& = "{}") override { return false; }
    void Play(double) override {}
    void Pause() override {}
    void SeekTo(uint64_t) override {}
    void SetSpeed(double) override {}
    void SetLoopRange(uint64_t, uint64_t) override {}
    void Stop() override {}
    uint64_t GetDuration() const override { return 0; }
    uint64_t GetCurrentPosition() const override { return 0; }
    double GetCurrentSpeed() const override { return 1.0; }

    void Emit(SessionState from, SessionState to) const {
        if (callback) {
            callback(from, to);
        }
    }

    StateCallback callback;
};

class FakeStatsService final : public IStatsService {
public:
    StatsSnapshot GetSnapshot() const override { return snapshot; }
    void OnUpdate(std::function<void(const StatsSnapshot&)> cb) override { callback = std::move(cb); }
    void RecordPacket(const std::string&, uint32_t, size_t) override {}
    void RecordDrop(const std::string&, uint64_t) override {}
    void RecordWriteLatency(double) override {}
    void UpdateRingBufferState(uint32_t, uint32_t) override {}
    void UpdateDiskQueue(uint64_t) override {}

    void Emit() const {
        if (callback) {
            callback(snapshot);
        }
    }

    StatsSnapshot snapshot;
    std::function<void(const StatsSnapshot&)> callback;
};

void TestLifecycleAndSubscription() {
    FakeSessionService session;
    FakeStatsService stats;
    WebSocketServer server;

    server.SetSessionService(&session);
    server.SetStatsService(&stats);
    Expect(server.Start(), "websocket server should start");
    Expect(server.IsRunning(), "websocket server should report running");

    stats.snapshot.total_packets = 12;
    stats.snapshot.total_drops = 1;
    stats.snapshot.cpu_usage_percent = 41.25;
    stats.Emit();
    session.Emit(SessionState::Idle, SessionState::Playing);

    const auto messages = server.GetBroadcastMessages();
    Expect(messages.size() == 2, "server should record stats and state broadcasts");
    Expect(messages[0].find("\"type\":\"stats\"") != std::string::npos, "first message should be stats");
    Expect(messages[0].find("\"total_packets\":12") != std::string::npos, "stats message should contain payload");
    Expect(messages[0].find("\"cpu_usage_percent\":41.25") != std::string::npos,
           "stats message should include actual cpu usage percent");
    Expect(messages[1].find("\"type\":\"state_changed\"") != std::string::npos, "second message should be state change");
    Expect(messages[1].find("\"old_state\":\"Idle\"") != std::string::npos, "state message should include old state");
    Expect(messages[1].find("\"new_state\":\"Playing\"") != std::string::npos, "state message should include new state");

    server.Stop();
    Expect(!server.IsRunning(), "websocket server should stop");
}

void TestManualBroadcastAndClear() {
    WebSocketServer server;
    server.Start();
    server.Broadcast("{\"type\":\"manual\"}");
    Expect(server.GetBroadcastMessages().size() == 1, "manual broadcast should be recorded");
    server.ClearBroadcastMessages();
    Expect(server.GetBroadcastMessages().empty(), "clear should remove recorded broadcasts");
    server.Stop();
}

void TestBroadcastUsesConfiguredTransport() {
    WebSocketServer server;
    std::string delivered;
    server.SetTransport([&](std::string_view message) {
        delivered.assign(message.begin(), message.end());
    });

    server.Start();
    server.Broadcast("{\"type\":\"manual-transport\"}");

    Expect(delivered == "{\"type\":\"manual-transport\"}",
           "transport should receive broadcast payload");

    server.Stop();
}

void TestSubscriptionsAreRefreshedAfterServiceInjection() {
    WebSocketServer server;
    Expect(server.Start(), "websocket server should start without services");

    FakeSessionService session;
    FakeStatsService stats;
    server.SetSessionService(&session);
    server.SetStatsService(&stats);

    stats.snapshot.total_packets = 5;
    stats.Emit();
    session.Emit(SessionState::Idle, SessionState::Playing);

    const auto messages = server.GetBroadcastMessages();
    Expect(messages.size() == 2,
           "server should subscribe to services injected after Start()");

    server.Stop();
}

void TestStaleSubscriptionsDoNotBroadcastAfterRebinding() {
    WebSocketServer server;
    FakeSessionService sessionA;
    FakeSessionService sessionB;
    FakeStatsService statsA;
    FakeStatsService statsB;

    Expect(server.Start(), "websocket server should start for rebinding test");

    server.SetSessionService(&sessionA);
    server.SetStatsService(&statsA);
    server.SetSessionService(&sessionB);
    server.SetStatsService(&statsB);

    statsA.snapshot.total_packets = 9;
    statsA.Emit();
    sessionA.Emit(SessionState::Idle, SessionState::Playing);
    Expect(server.GetBroadcastMessages().empty(),
           "stale services should not broadcast after rebinding");

    statsB.snapshot.total_packets = 10;
    statsB.Emit();
    sessionB.Emit(SessionState::Idle, SessionState::Playing);

    const auto messages = server.GetBroadcastMessages();
    Expect(messages.size() == 2, "current services should still broadcast after rebinding");

    server.Stop();
}

void TestStatsBroadcastSanitizesNonFiniteNumbers() {
    FakeSessionService session;
    FakeStatsService stats;
    WebSocketServer server(&session, &stats);

    Expect(server.Start(), "websocket server should start for non-finite stats test");

    stats.snapshot.total_packets = 1;
    stats.snapshot.total_drops = 2;
    stats.snapshot.total_throughput_mbps = std::numeric_limits<double>::infinity();
    stats.snapshot.cpu_usage_percent = std::numeric_limits<double>::quiet_NaN();
    stats.Emit();

    const auto messages = server.GetBroadcastMessages();
    Expect(messages.size() == 1, "server should emit one stats message");
    Expect(messages[0].find("\"total_throughput_mbps\":0") != std::string::npos,
           "stats message should sanitize non-finite throughput");
    Expect(messages[0].find("\"cpu_usage_percent\":null") != std::string::npos,
           "stats message should serialize non-finite cpu usage as null");

    server.Stop();
}

void TestBroadcastHistoryHasBoundedSize() {
    WebSocketServer server;
    Expect(server.Start(), "websocket server should start for bounded history test");

    for (size_t i = 0; i < 300; ++i) {
        server.Broadcast("{\"type\":\"manual\",\"seq\":" + std::to_string(i) + "}");
    }

    const auto messages = server.GetBroadcastMessages();
    Expect(messages.size() == 256, "broadcast history should be bounded");
    Expect(messages.front().find("\"seq\":44") != std::string::npos,
           "oldest retained message should reflect trimmed history");
    Expect(messages.back().find("\"seq\":299") != std::string::npos,
           "newest message should remain available");

    server.Stop();
}

} // namespace

int main() {
    try {
        TestLifecycleAndSubscription();
        TestManualBroadcastAndClear();
        TestBroadcastUsesConfiguredTransport();
        TestSubscriptionsAreRefreshedAfterServiceInjection();
        TestStaleSubscriptionsDoNotBroadcastAfterRebinding();
        TestStatsBroadcastSanitizesNonFiniteNumbers();
        TestBroadcastHistoryHasBoundedSize();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
