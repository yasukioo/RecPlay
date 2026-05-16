// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "TcpProtocolService.h"

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
#include <mutex>
#include <optional>
#include <thread>
#include <stdexcept>
#include <string>

namespace {

using recplay::TcpProtocolService;

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

void TestSchemaIncludesTcpFields() {
    TcpProtocolService service;
    const std::string schema = service.GetConfigSchema();

    Expect(schema.find("\"mode\"") != std::string::npos, "schema should include mode: " + schema);
    Expect(schema.find("\"host\"") != std::string::npos, "schema should include host: " + schema);
    Expect(schema.find("\"port\"") != std::string::npos, "schema should include port: " + schema);
}

void TestCaptureLifecycleAndChannels() {
    TcpProtocolService service;
    const std::string config =
        R"({"mode":"server","host":"127.0.0.1","port":0,"channel_id":11,"channel_name":"tcp-test","topic":"tcp/topic"})";

    const bool started = service.StartCapture(config, [](recplay::PacketPtr) {});

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    Expect(started, "capture should start when asio and json are available");
    Expect(service.IsCapturing(), "service should report capturing");
    const auto channels = service.GetChannels();
    Expect(channels.size() == 1, "capture config should expose one channel");
    Expect(channels.front().id == 11, "channel id should come from config");
    Expect(channels.front().name == "tcp-test", "channel name should come from config");
    Expect(channels.front().protocol == "TCP", "channel protocol should be TCP");
    Expect(channels.front().topic == "tcp/topic", "channel topic should come from config");
    service.StopCapture();
    Expect(!service.IsCapturing(), "service should stop capturing");
#else
    Expect(!started, "capture should fail cleanly when asio/json are unavailable");
    Expect(!service.IsCapturing(), "service should remain stopped without dependencies");
#endif
}

void TestReplayLifecycle() {
    TcpProtocolService service;
    const std::string config =
        R"({"mode":"client","host":"127.0.0.1","port":9})";

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
    TcpProtocolService service;
    const std::string captureConfig =
        R"({"mode":"server","host":"127.0.0.1","port":0,"channel_id":11,"channel_name":"tcp-test","topic":"tcp/topic"})";
    const std::string replayConfig =
        R"({"mode":"client","host":"127.0.0.1","port":9})";

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

void TestCapturedPacketsPopulateTimestamps() {
#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    TcpProtocolService service;
    const unsigned short port = 39012;
    const std::string config =
        R"({"mode":"server","host":"127.0.0.1","port":39012,"channel_id":8,"channel_name":"tcp-ts","topic":"tcp/timestamps"})";

    std::mutex mutex;
    std::optional<recplay::Packet> captured;
    Expect(service.StartCapture(config, [&](recplay::PacketPtr packet) {
        std::lock_guard<std::mutex> lock(mutex);
        captured = *packet;
    }), "capture should start for timestamp test");

    boost::asio::io_context ioContext;
    boost::asio::ip::tcp::socket socket(ioContext);
    socket.connect(boost::asio::ip::tcp::endpoint(
        boost::asio::ip::make_address("127.0.0.1"), port));
    const std::array<uint8_t, 4> payload{{0xaa, 0xbb, 0xcc, 0xdd}};
    boost::asio::write(socket, boost::asio::buffer(payload));

    Expect(WaitUntil([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return captured.has_value();
    }, std::chrono::milliseconds(500)), "expected TCP packet callback");

    recplay::Packet packet;
    {
        std::lock_guard<std::mutex> lock(mutex);
        packet = *captured;
    }

    Expect(packet.t_capture != 0, "captured TCP packet should set t_capture");
    Expect(packet.t_origin != 0, "captured TCP packet should set t_origin");
    Expect(packet.t_record != 0, "captured TCP packet should set t_record");

    service.StopCapture();
#endif
}

void TestServerCaptureAcceptsSequentialConnections() {
#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    TcpProtocolService service;
    const unsigned short port = 39013;
    const std::string config =
        R"({"mode":"server","host":"127.0.0.1","port":39013,"channel_id":12,"channel_name":"tcp-accept","topic":"tcp/accept"})";

    std::mutex mutex;
    size_t packetCount = 0;
    Expect(service.StartCapture(config, [&](recplay::PacketPtr) {
        std::lock_guard<std::mutex> lock(mutex);
        ++packetCount;
    }), "capture should start for sequential accept test");

    auto sendPayload = [&](uint8_t firstByte) {
        boost::asio::io_context ioContext;
        boost::asio::ip::tcp::socket socket(ioContext);
        socket.connect(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address("127.0.0.1"), port));
        const std::array<uint8_t, 4> payload{{firstByte, 0x02, 0x03, 0x04}};
        boost::asio::write(socket, boost::asio::buffer(payload));
        boost::system::error_code ec;
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket.close(ec);
    };

    sendPayload(0x11);
    Expect(WaitUntil([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return packetCount >= 1;
    }, std::chrono::milliseconds(500)), "first TCP client should be received");

    sendPayload(0x22);
    Expect(WaitUntil([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return packetCount >= 2;
    }, std::chrono::milliseconds(500)), "second TCP client should also be received");

    service.StopCapture();
#endif
}

} // namespace

int main() {
    try {
        TestSchemaIncludesTcpFields();
        TestCaptureLifecycleAndChannels();
        TestReplayLifecycle();
        TestCaptureAndReplayStopIndependently();
        TestCapturedPacketsPopulateTimestamps();
        TestServerCaptureAcceptsSequentialConnections();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
