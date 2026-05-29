// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "ISessionService.h"
#include "IStatsService.h"
#include "RecPlayWebSocketController.h"
#include "WebSocketServer.h"

#if __has_include(<drogon/WebSocketClient.h>)
#include <drogon/HttpRequest.h>
#include <drogon/WebSocketClient.h>
#define RECPLAY_TEST_HAS_DROGON_WS_CLIENT 1
#else
#define RECPLAY_TEST_HAS_DROGON_WS_CLIENT 0
#endif

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using recplay::HttpServer;
using recplay::IStatsService;
using recplay::ISessionService;
using recplay::RecPlayWebSocketController;
using recplay::SessionState;
using recplay::StateCallback;
using recplay::StatsSnapshot;
using recplay::WebSocketServer;

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

void TestWebSocketTransportDeliversStatsBroadcast() {
#if RECPLAY_TEST_HAS_DROGON_WS_CLIENT
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer httpServer(&session, &stats);
    auto websocketServer = std::make_shared<WebSocketServer>(&session, &stats);
    RecPlayWebSocketController::SetServer(websocketServer);
    websocketServer->SetTransport([](std::string_view message) {
        RecPlayWebSocketController::Broadcast(message);
    });

    Expect(httpServer.Start("127.0.0.1", 18080), "http server should start");
    Expect(websocketServer->Start(), "websocket server should start");

    std::mutex mutex;
    std::string receivedMessage;
    std::atomic<bool> connected{false};

    auto client = drogon::WebSocketClient::newWebSocketClient("ws://127.0.0.1:18080");
    client->setMessageHandler([&](std::string&& message,
                                  const drogon::WebSocketClientPtr&,
                                  const drogon::WebSocketMessageType&) {
        std::lock_guard<std::mutex> lock(mutex);
        receivedMessage = std::move(message);
    });

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setPath("/api/ws");
    client->connectToServer(request, [&](drogon::ReqResult result,
                                         const drogon::HttpResponsePtr&,
                                         const drogon::WebSocketClientPtr&) {
        connected.store(result == drogon::ReqResult::Ok, std::memory_order_release);
    });

    Expect(WaitUntil([&] {
        return connected.load(std::memory_order_acquire);
    }, std::chrono::milliseconds(1000)), "websocket client should connect");

    stats.snapshot.total_packets = 77;
    stats.snapshot.total_drops = 2;
    stats.snapshot.total_throughput_mbps = 5.5;
    stats.Emit();

    Expect(WaitUntil([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return receivedMessage.find("\"type\":\"stats\"") != std::string::npos;
    }, std::chrono::milliseconds(1000)), "websocket client should receive stats broadcast");

    {
        std::lock_guard<std::mutex> lock(mutex);
        Expect(receivedMessage.find("\"total_packets\":77") != std::string::npos,
               "broadcast should include stats payload");
    }

    client->stop();
    websocketServer->Stop();
    httpServer.Stop();
    websocketServer->SetTransport({});
    RecPlayWebSocketController::SetServer(nullptr);
#endif
}

} // namespace

int main() {
    try {
        TestWebSocketTransportDeliversStatsBroadcast();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
