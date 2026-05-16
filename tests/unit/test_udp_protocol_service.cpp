// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "UdpProtocolService.h"

#if __has_include(<boost/asio.hpp>)
#define RECPLAY_TEST_HAS_BOOST_ASIO 1
#else
#define RECPLAY_TEST_HAS_BOOST_ASIO 0
#endif

#if __has_include(<nlohmann/json.hpp>)
#define RECPLAY_TEST_HAS_JSON 1
#else
#define RECPLAY_TEST_HAS_JSON 0
#endif

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using recplay::UdpProtocolService;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestSchemaIncludesUdpFields() {
    UdpProtocolService service;
    const std::string schema = service.GetConfigSchema();

    Expect(schema.find("\"address\"") != std::string::npos, "schema should include address: " + schema);
    Expect(schema.find("\"port\"") != std::string::npos, "schema should include port: " + schema);
    Expect(schema.find("\"interface\"") != std::string::npos, "schema should include interface: " + schema);
    Expect(schema.find("\"recv_buf\"") != std::string::npos, "schema should include recv_buf: " + schema);
}

void TestCaptureLifecycleAndChannels() {
    UdpProtocolService service;
    const std::string config =
        R"({"address":"127.0.0.1","interface":"0.0.0.0","port":0,"recv_buf":4096,"channel_id":7,"channel_name":"udp-test","topic":"demo/topic"})";

    const bool started = service.StartCapture(config, [](recplay::PacketPtr) {});

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    Expect(started, "capture should start when asio and json are available");
    Expect(service.IsCapturing(), "service should report capturing");
    const auto channels = service.GetChannels();
    Expect(channels.size() == 1, "capture config should expose one channel");
    Expect(channels.front().id == 7, "channel id should come from config");
    Expect(channels.front().name == "udp-test", "channel name should come from config");
    Expect(channels.front().protocol == "UDP", "channel protocol should be UDP");
    Expect(channels.front().topic == "demo/topic", "channel topic should come from config");
    service.StopCapture();
    Expect(!service.IsCapturing(), "service should stop capturing");
#else
    Expect(!started, "capture should fail cleanly when asio/json are unavailable");
    Expect(!service.IsCapturing(), "service should remain stopped without dependencies");
#endif
}

void TestReplayLifecycle() {
    UdpProtocolService service;
    const std::string config =
        R"({"address":"127.0.0.1","interface":"0.0.0.0","port":9})";

    const bool started = service.StartReplay(config);

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    Expect(started, "replay should start when asio and json are available");
    Expect(service.IsReplaying(), "service should report replaying");
    Expect(!service.SendPacket(nullptr), "null packets should be rejected");
    service.StopReplay();
    Expect(!service.IsReplaying(), "service should stop replaying");
#else
    Expect(!started, "replay should fail cleanly when asio/json are unavailable");
    Expect(!service.IsReplaying(), "service should remain stopped without dependencies");
#endif
}

void TestCaptureAndReplayStopIndependently() {
    UdpProtocolService service;
    const std::string captureConfig =
        R"({"address":"127.0.0.1","interface":"0.0.0.0","port":0,"channel_id":1})";
    const std::string replayConfig =
        R"({"address":"127.0.0.1","interface":"0.0.0.0","port":9})";

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    Expect(service.StartReplay(replayConfig), "replay should start for independence test");
    Expect(service.StartCapture(captureConfig, [](recplay::PacketPtr) {}),
           "capture should start for independence test");
    Expect(service.IsReplaying(), "replay should be active before capture stop");
    Expect(service.IsCapturing(), "capture should be active before capture stop");

    service.StopCapture();
    Expect(!service.IsCapturing(), "capture stop should stop capture");
    Expect(service.IsReplaying(), "capture stop should not stop replay");

    service.StopReplay();
    Expect(!service.IsReplaying(), "replay stop should stop replay");
#else
    Expect(!service.StartReplay(replayConfig), "replay should fail cleanly without dependencies");
    Expect(!service.StartCapture(captureConfig, [](recplay::PacketPtr) {}),
           "capture should fail cleanly without dependencies");
#endif
}

} // namespace

int main() {
    try {
        TestSchemaIncludesUdpFields();
        TestCaptureLifecycleAndChannels();
        TestReplayLifecycle();
        TestCaptureAndReplayStopIndependently();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
