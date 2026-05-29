// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "ISessionService.h"
#include "IStatsService.h"

#if __has_include(<drogon/HttpClient.h>)
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#define RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT 1
#else
#define RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT 0
#endif

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using recplay::HttpServer;
using recplay::ISessionService;
using recplay::IStatsService;
using recplay::SessionState;
using recplay::StateCallback;
using recplay::StatsSnapshot;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Predicate>
bool WaitUntil(Predicate&& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

class FakeSessionService final : public ISessionService {
public:
    SessionState GetState() const override { return state; }
    void OnStateChanged(StateCallback cb) override { callback = std::move(cb); }

    bool StartRecording(const std::string& configJson) override {
        last_record_config = configJson;
        return start_record_result;
    }

    void PauseRecording() override {}
    void ResumeRecording() override {}
    void StopRecording() override {}

    bool OpenForPlayback(const std::string& filePath, const std::string& = "{}") override {
        last_open_path = filePath;
        return open_for_playback_result;
    }

    void Play(double newSpeed) override { speed = newSpeed; }
    void Pause() override {}
    void SeekTo(uint64_t timestamp_ns) override { position_ns = timestamp_ns; }
    void SetSpeed(double newSpeed) override { speed = newSpeed; }
    void SetLoopRange(uint64_t, uint64_t) override {}
    void Stop() override {}

    uint64_t GetDuration() const override { return duration_ns; }
    uint64_t GetCurrentPosition() const override { return position_ns; }
    double GetCurrentSpeed() const override { return speed; }

    SessionState state = SessionState::Idle;
    uint64_t duration_ns = 321'000'000ULL;
    uint64_t position_ns = 123'000'000ULL;
    double speed = 1.25;
    bool start_record_result = true;
    bool open_for_playback_result = true;
    std::string last_record_config;
    std::string last_open_path;
    StateCallback callback;
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

#if RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT
struct RequestResult {
    drogon::ReqResult result = drogon::ReqResult::NetworkFailure;
    int status_code = 0;
    std::string body;
};

RequestResult SendRequest(int port,
                          drogon::HttpMethod method,
                          const std::string& path,
                          const std::string& body = {}) {
    std::atomic<bool> done{false};
    RequestResult output;

    auto client =
        drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(port));
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(method);
    request->setPath(path);
    if (!body.empty()) {
        request->setBody(body);
    }

    client->sendRequest(
        request,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
            output.result = result;
            output.status_code = response != nullptr ? static_cast<int>(response->statusCode()) : 0;
            output.body = response != nullptr ? std::string(response->getBody()) : std::string{};
            done.store(true, std::memory_order_release);
        });

    Expect(WaitUntil([&] {
               return done.load(std::memory_order_acquire);
           }, std::chrono::milliseconds(1000)),
           "HTTP request timed out for path " + path);
    return output;
}

bool RequestHealth(int port) {
    const auto response = SendRequest(port, drogon::Get, "/api/health");
    return response.result == drogon::ReqResult::Ok &&
        response.status_code == 200 &&
        response.body == "{\"status\":\"ok\"}";
}
#endif

void TestWebServerRoutesRespondOverRealHttp() {
#if RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT
    FakeSessionService session;
    session.state = SessionState::PlaybackPaused;

    FakeStatsService stats;
    stats.snapshot.total_packets = 77;
    stats.snapshot.total_drops = 2;
    stats.snapshot.total_throughput_mbps = 5.5;
    stats.snapshot.cpu_usage_percent = 12.5;

    HttpServer server(&session, &stats);
    constexpr int kPort = 18082;

    Expect(server.Start("127.0.0.1", kPort), "web server should start for integration test");
    Expect(WaitUntil([&] {
               return RequestHealth(kPort);
           }, std::chrono::milliseconds(1000)),
           "health endpoint should become reachable after server start");

    const auto session_state = SendRequest(kPort, drogon::Get, "/api/session/state");
    Expect(session_state.result == drogon::ReqResult::Ok, "session state request should complete");
    Expect(session_state.status_code == 200, "session state route should return HTTP 200");
    Expect(session_state.body.find("\"state\":\"PlaybackPaused\"") != std::string::npos,
           "session state body should include the current state");
    Expect(session_state.body.find("\"duration_ns\":321000000") != std::string::npos,
           "session state body should include duration");
    Expect(session_state.body.find("\"position_ns\":123000000") != std::string::npos,
           "session state body should include position");

    const auto stats_response = SendRequest(kPort, drogon::Get, "/api/stats");
    Expect(stats_response.result == drogon::ReqResult::Ok, "stats request should complete");
    Expect(stats_response.status_code == 200, "stats route should return HTTP 200");
    Expect(stats_response.body.find("\"total_packets\":77") != std::string::npos,
           "stats body should include packet count");
    Expect(stats_response.body.find("\"cpu_usage_percent\":12.5") != std::string::npos,
           "stats body should include optional CPU usage");

    const auto missing_file = SendRequest(kPort, drogon::Post, "/api/session/playback/open", "{}");
    Expect(missing_file.result == drogon::ReqResult::Ok, "invalid playback-open request should still complete");
    Expect(missing_file.status_code == 400, "missing file_path should return HTTP 400");
    Expect(missing_file.body.find("file_path is required") != std::string::npos,
           "invalid playback-open body should explain the missing file_path");

    const auto open_playback = SendRequest(
        kPort,
        drogon::Post,
        "/api/session/playback/open",
        R"({"file_path":"capture.rpcap"})");
    Expect(open_playback.result == drogon::ReqResult::Ok, "playback-open request should complete");
    Expect(open_playback.status_code == 200, "playback-open should return HTTP 200 when the session accepts it");
    Expect(session.last_open_path == "capture.rpcap",
           "session service should receive the playback path through the real HTTP route");

    server.Stop();
#endif
}

} // namespace

int main() {
    try {
        TestWebServerRoutesRespondOverRealHttp();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
