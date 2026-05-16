// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "ISessionService.h"
#include "IStatsService.h"

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using recplay::HttpServer;
using recplay::IStatsService;
using recplay::ISessionService;
using recplay::SessionState;
using recplay::StateCallback;
using recplay::StatsSnapshot;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeSessionService final : public ISessionService {
public:
    SessionState GetState() const override { return state; }
    void OnStateChanged(StateCallback cb) override { callback = std::move(cb); }
    bool StartRecording(const std::string& config) override {
        start_record_called = true;
        last_record_config = config;
        return start_record_result;
    }
    void PauseRecording() override { pause_record_called = true; }
    void ResumeRecording() override { resume_record_called = true; }
    void StopRecording() override { stop_record_called = true; }
    bool OpenForPlayback(const std::string& path) override {
        open_playback_called = true;
        last_open_path = path;
        return open_playback_result;
    }
    void Play(double value) override {
        play_called = true;
        last_play_speed = value;
    }
    void Pause() override { pause_called = true; }
    void SeekTo(uint64_t value) override {
        seek_called = true;
        last_seek_ns = value;
    }
    void SetSpeed(double value) override {
        set_speed_called = true;
        speed = value;
    }
    void SetLoopRange(uint64_t start, uint64_t end) override {
        loop_called = true;
        last_loop_start_ns = start;
        last_loop_end_ns = end;
    }
    void Stop() override { stop_called = true; }
    uint64_t GetDuration() const override { return duration; }
    uint64_t GetCurrentPosition() const override { return position; }
    double GetCurrentSpeed() const override { return speed; }

    SessionState state = SessionState::Playing;
    uint64_t duration = 1000;
    uint64_t position = 250;
    double speed = 2.0;
    StateCallback callback;
    double last_play_speed = 0.0;
    uint64_t last_seek_ns = 0;
    uint64_t last_loop_start_ns = 0;
    uint64_t last_loop_end_ns = 0;
    std::string last_record_config;
    std::string last_open_path;
    bool play_called = false;
    bool pause_called = false;
    bool seek_called = false;
    bool loop_called = false;
    bool stop_called = false;
    bool set_speed_called = false;
    bool start_record_called = false;
    bool pause_record_called = false;
    bool resume_record_called = false;
    bool stop_record_called = false;
    bool open_playback_called = false;
    bool start_record_result = true;
    bool open_playback_result = true;
};

class FakeStatsService final : public IStatsService {
public:
    StatsSnapshot GetSnapshot() const override { return snapshot; }
    void OnUpdate(std::function<void(const StatsSnapshot&)>) override {}
    void RecordPacket(const std::string&, uint32_t, size_t) override {}
    void RecordDrop(const std::string&, uint64_t) override {}
    void RecordWriteLatency(double) override {}
    void UpdateRingBufferState(uint32_t, uint32_t) override {}
    void UpdateDiskQueue(uint64_t) override {}

    StatsSnapshot snapshot;
};

void TestSessionStateResponse() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest("GET", "/api/session/state", "");
    Expect(response.status_code == 200, "session state endpoint should return 200");
    Expect(response.content_type == "application/json", "session state should be json");
    Expect(response.body.find("\"state\":\"Playing\"") != std::string::npos, "body should include state");
    Expect(response.body.find("\"duration_ns\":1000") != std::string::npos, "body should include duration");
    Expect(response.body.find("\"position_ns\":250") != std::string::npos, "body should include position");
    Expect(response.body.find("\"speed\":2") != std::string::npos, "body should include speed");
}

void TestStatsSnapshotResponse() {
    FakeSessionService session;
    FakeStatsService stats;
    stats.snapshot.total_packets = 42;
    stats.snapshot.total_drops = 3;
    stats.snapshot.total_throughput_mbps = 12.5;
    stats.snapshot.write_latency_p99_ms = 4.75;
    stats.snapshot.ringbuf_used = 7;
    stats.snapshot.ringbuf_capacity = 64;
    stats.snapshot.disk_queue_bytes = 8192;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 200, "stats endpoint should return 200");
    Expect(response.body.find("\"total_packets\":42") != std::string::npos, "body should include packet count");
    Expect(response.body.find("\"total_drops\":3") != std::string::npos, "body should include drop count");
    Expect(response.body.find("\"total_throughput_mbps\":12.5") != std::string::npos, "body should include throughput");
    Expect(response.body.find("\"write_latency_p99_ms\":4.75") != std::string::npos, "body should include p99");
    Expect(response.body.find("\"ringbuf_used\":7") != std::string::npos, "body should include ring buffer");
    Expect(response.body.find("\"disk_queue_bytes\":8192") != std::string::npos, "body should include disk queue");
}

void TestUnknownRouteResponse() {
    HttpServer server(nullptr, nullptr);
    const auto response = server.HandleRequest("GET", "/api/unknown", "");
    Expect(response.status_code == 404, "unknown route should return 404");
}

void TestUnavailableServicesReturn503() {
    HttpServer server(nullptr, nullptr);

    auto response = server.HandleRequest("GET", "/api/session/state", "");
    Expect(response.status_code == 503, "session route should return 503 without service");

    response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 503, "stats route should return 503 without service");

    FakeSessionService session;
    FakeStatsService stats;
    server.SetSessionService(&session);
    server.SetStatsService(&stats);

    response = server.HandleRequest("GET", "/api/session/state", "");
    Expect(response.status_code == 200, "session route should recover after injection");

    response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 200, "stats route should recover after injection");
}

void TestPlaybackCommandRoutes() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest("POST", "/api/session/playback/play", R"({"speed":1.5})");
    Expect(response.status_code == 200,
           "play route should return 200: code=" + std::to_string(response.status_code) +
           ", body=" + response.body);
    Expect(session.play_called, "play route should call session service");
    Expect(session.last_play_speed == 1.5, "play route should forward speed");

    response = server.HandleRequest("POST", "/api/session/playback/pause", "");
    Expect(response.status_code == 200, "pause route should return 200");
    Expect(session.pause_called, "pause route should call session service");

    response = server.HandleRequest("POST", "/api/session/playback/seek", R"({"timestamp_ns":400})");
    Expect(response.status_code == 200, "seek route should return 200");
    Expect(session.seek_called, "seek route should call session service");
    Expect(session.last_seek_ns == 400, "seek route should forward timestamp");

    response = server.HandleRequest("POST", "/api/session/playback/play", R"({"speed":1e3})");
    Expect(response.status_code == 200, "play route should accept exponent-form speed");
    Expect(session.last_play_speed == 1000.0, "play route should forward exponent-form speed");

    response = server.HandleRequest("POST", "/api/session/playback/seek", R"({"timestamp_ns":1e3})");
    Expect(response.status_code == 400, "seek route should reject exponent-form timestamp");
    Expect(session.last_seek_ns == 400, "seek route should not update timestamp on exponent-form input");

    response = server.HandleRequest("POST", "/api/session/playback/stop", "");
    Expect(response.status_code == 200, "stop route should return 200");
    Expect(session.stop_called, "stop route should call session service");
}

void TestPlaybackValidationErrors() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"timestamp_ns":"soon"})");
    Expect(response.status_code == 400, "seek route should reject non-numeric timestamp");
    Expect(!session.seek_called, "seek route should not call session service on invalid timestamp");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"timestamp_ns":-1})");
    Expect(response.status_code == 400, "seek route should reject negative timestamp");
    Expect(!session.seek_called, "seek route should not call session service on negative timestamp");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/play",
        R"({"speed":"fast"})");
    Expect(response.status_code == 400, "play route should reject non-numeric speed");
    Expect(!session.play_called, "play route should not call session service on invalid speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"speed":"fast"})");
    Expect(response.status_code == 400, "speed route should reject non-numeric speed");
    Expect(!session.set_speed_called, "speed route should not call session service on invalid speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"speed":1e3})");
    Expect(response.status_code == 200, "speed route should accept exponent-form speed");
    Expect(session.speed == 1000.0, "speed route should forward exponent-form speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":"early","end_ns":300})");
    Expect(response.status_code == 400, "loop route should reject non-numeric start");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":-1,"end_ns":300})");
    Expect(response.status_code == 400, "loop route should reject negative start");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":100,"end_ns":-1})");
    Expect(response.status_code == 400, "loop route should reject negative end");
}

void TestPlaybackNumericRoutesIgnoreSubstringAndNestedKeys() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/play",
        R"({"note":"speed","other":3.5})");
    Expect(response.status_code == 200, "play route should ignore speed token in string values");
    Expect(session.play_called, "play route should still call session service when speed is omitted");
    Expect(session.last_play_speed == 1.0, "play route should keep default speed when top-level speed is absent");

    session.play_called = false;
    session.last_play_speed = 0.0;
    response = server.HandleRequest(
        "POST",
        "/api/session/playback/play",
        R"({"playback":{"speed":2.5}})");
    Expect(response.status_code == 200, "play route should ignore nested speed fields");
    Expect(session.play_called, "play route should still call session service when nested speed is ignored");
    Expect(session.last_play_speed == 1.0, "play route should keep default speed when only nested speed exists");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"note":"speed","other":4.0})");
    Expect(response.status_code == 400, "speed route should reject missing top-level speed");
    Expect(!session.set_speed_called, "speed route should not use a sibling field after a string token match");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"playback":{"speed":4.0}})");
    Expect(response.status_code == 400, "speed route should reject nested speed");
    Expect(!session.set_speed_called, "speed route should not use nested speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"note":"timestamp_ns","other":400})");
    Expect(response.status_code == 400, "seek route should reject missing top-level timestamp");
    Expect(!session.seek_called, "seek route should not use a sibling field after a string token match");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"playback":{"timestamp_ns":400}})");
    Expect(response.status_code == 400, "seek route should reject nested timestamp");
    Expect(!session.seek_called, "seek route should not use nested timestamp");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"note":"start_ns","other":100,"end_ns":300})");
    Expect(response.status_code == 400, "loop route should reject missing top-level start");
    Expect(!session.loop_called, "loop route should not use a sibling field after a string token match");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"playback":{"start_ns":100,"end_ns":300}})");
    Expect(response.status_code == 400, "loop route should reject nested loop bounds");
    Expect(!session.loop_called, "loop route should not use nested loop bounds");
}

void TestPlaybackInvalidStateResponses() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    session.state = SessionState::Idle;

    const auto response = server.HandleRequest("POST", "/api/session/playback/pause", "");
    Expect(response.status_code == 409, "pause route should reject idle playback state");
    Expect(!session.pause_called, "pause route should not call session service when state is idle");
}

void TestRemainingSessionRoutes() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest(
        "POST",
        "/api/session/record",
        R"({"output_path":"capture.rpcap","protocols":["UDP"]})");
    Expect(response.status_code == 200,
           "record route should return 200: code=" + std::to_string(response.status_code) +
           ", body=" + response.body);
    Expect(session.start_record_called, "record route should call StartRecording");
    Expect(session.last_record_config.find("\"output_path\":\"capture.rpcap\"") != std::string::npos,
           "record route should forward config body");

    response = server.HandleRequest("POST", "/api/session/record/pause", "");
    Expect(response.status_code == 200, "record pause route should return 200");
    Expect(session.pause_record_called, "record pause should call PauseRecording");

    response = server.HandleRequest("POST", "/api/session/record/resume", "");
    Expect(response.status_code == 200, "record resume route should return 200");
    Expect(session.resume_record_called, "record resume should call ResumeRecording");

    response = server.HandleRequest("POST", "/api/session/record/stop", "");
    Expect(response.status_code == 200, "record stop route should return 200");
    Expect(session.stop_record_called, "record stop should call StopRecording");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/open",
        R"({"file_path":"capture.rpcap"})");
    Expect(response.status_code == 200, "playback open route should return 200");
    Expect(session.open_playback_called, "playback open should call OpenForPlayback");
    Expect(session.last_open_path == "capture.rpcap", "playback open should forward file path");

    response = server.HandleRequest("POST", "/api/session/playback/speed", R"({"speed":4.0})");
    Expect(response.status_code == 200, "playback speed route should return 200");
    Expect(session.set_speed_called, "playback speed should call SetSpeed");
    Expect(session.speed == 4.0, "playback speed should forward speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":100,"end_ns":300})");
    Expect(response.status_code == 200, "playback loop route should return 200");
}

void TestErrorResponseEscapesJsonMessage() {
    class FailingSessionService final : public ISessionService {
    public:
        SessionState GetState() const override { return SessionState::Idle; }
        void OnStateChanged(StateCallback cb) override { callback = std::move(cb); }
        bool StartRecording(const std::string&) override { return false; }
        void PauseRecording() override {}
        void ResumeRecording() override {}
        void StopRecording() override {}
        bool OpenForPlayback(const std::string& path) override {
            last_open_path = path;
            return false;
        }
        void Play(double) override {}
        void Pause() override {}
        void SeekTo(uint64_t) override {}
        void SetSpeed(double) override {}
        void SetLoopRange(uint64_t, uint64_t) override {}
        void Stop() override {}
        uint64_t GetDuration() const override { return 0; }
        uint64_t GetCurrentPosition() const override { return 0; }
        double GetCurrentSpeed() const override { return 1.0; }

        StateCallback callback;
        std::string last_open_path;
    };

    FailingSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/open",
        R"({"file_path":"broken\"name.rpcap"})");

    Expect(response.status_code == 400, "failed playback open should return 400");
    Expect(response.body.find("\\\"") != std::string::npos,
           "error response should escape embedded quotes in JSON body: " + response.body);
}

void TestRunningStateTransitions() {
    HttpServer server(nullptr, nullptr);
    Expect(!server.IsRunning(), "server should start stopped");
    Expect(server.Start("127.0.0.1", 0), "start should succeed");
    Expect(server.IsRunning(), "server should report running after start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after stop");

    Expect(server.Start("127.0.0.1", 0), "second start should succeed");
    Expect(server.IsRunning(), "server should report running after second start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after second stop");
}

} // namespace

int main() {
    try {
        TestSessionStateResponse();
        TestStatsSnapshotResponse();
        TestUnknownRouteResponse();
        TestUnavailableServicesReturn503();
        TestPlaybackCommandRoutes();
        TestPlaybackValidationErrors();
        TestPlaybackNumericRoutesIgnoreSubstringAndNestedKeys();
        TestPlaybackInvalidStateResponses();
        TestRemainingSessionRoutes();
        TestErrorResponseEscapesJsonMessage();
        TestRunningStateTransitions();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
