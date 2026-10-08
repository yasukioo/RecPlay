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
#include <atomic>
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

#if RECPLAY_TEST_HAS_BOOST_ASIO
class LoopbackTcpServer {
public:
    LoopbackTcpServer()
        : acceptor_(
            io_context_,
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address("127.0.0.1"),
                0)) {}

    ~LoopbackTcpServer() { Stop(); }

    unsigned short Port() const {
        return acceptor_.local_endpoint().port();
    }

    void Start(size_t expectedConnections = 1) {
        thread_ = std::thread([this, expectedConnections] {
            for (size_t i = 0; i < expectedConnections; ++i) {
                boost::asio::ip::tcp::socket socket(io_context_);
                boost::system::error_code acceptError;
                acceptor_.accept(socket, acceptError);
                if (acceptError) {
                    return;
                }
                connections_.fetch_add(1, std::memory_order_acq_rel);

                std::array<uint8_t, 256> buffer{};
                while (true) {
                    boost::system::error_code readError;
                    const auto bytesRead = socket.read_some(boost::asio::buffer(buffer), readError);
                    bytes_received_.fetch_add(bytesRead, std::memory_order_acq_rel);
                    if (readError == boost::asio::error::eof || bytesRead == 0) {
                        break;
                    }
                    if (readError) {
                        return;
                    }
                }
            }
        });
    }

    void Stop() {
        boost::system::error_code ec;
        acceptor_.cancel(ec);
        acceptor_.close(ec);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    size_t ConnectionCount() const {
        return connections_.load(std::memory_order_acquire);
    }

    size_t BytesReceived() const {
        return bytes_received_.load(std::memory_order_acquire);
    }

private:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::thread thread_;
    std::atomic<size_t> connections_{0};
    std::atomic<size_t> bytes_received_{0};
};
#endif

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

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    LoopbackTcpServer replayServer;
    replayServer.Start();
    const std::string config =
        std::string(R"({"mode":"client","host":"127.0.0.1","port":)") +
        std::to_string(replayServer.Port()) +
        "}";
    const bool started = service.StartReplay(config);
    Expect(started, "replay should start when asio and json are available");
    Expect(service.IsReplaying(), "service should report replaying");
    Expect(!service.SendPacket(nullptr), "null packets should be rejected");
    service.StopReplay();
    Expect(!service.IsReplaying(), "service should stop replaying");
    replayServer.Stop();
#else
    const std::string config =
        R"({"mode":"client","host":"127.0.0.1","port":9})";
    const bool started = service.StartReplay(config);
    Expect(!started, "replay should fail cleanly when asio/json are unavailable");
    Expect(!service.IsReplaying(), "service should remain stopped without dependencies");
#endif
}

void TestReplayFanoutSupportsMultipleTargets() {
#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    TcpProtocolService service;
    LoopbackTcpServer serverA;
    LoopbackTcpServer serverB;
    serverA.Start();
    serverB.Start();

    const std::string config =
        std::string("[") +
        R"({"mode":"client","host":"127.0.0.1","port":)" + std::to_string(serverA.Port()) + "}," +
        R"({"mode":"client","host":"127.0.0.1","port":)" + std::to_string(serverB.Port()) + "}]" ;

    Expect(service.StartReplay(config), "multi-target TCP replay should start");

    auto packet = std::make_shared<recplay::Packet>();
    packet->payload = {0xaa, 0xbb, 0xcc, 0xdd};
    Expect(service.SendPacket(packet), "multi-target TCP replay should send packet to all targets");

    Expect(WaitUntil([&] { return serverA.ConnectionCount() >= 1 && serverA.BytesReceived() >= 4; }, std::chrono::milliseconds(500)),
           "first TCP target should receive replay payload");
    Expect(WaitUntil([&] { return serverB.ConnectionCount() >= 1 && serverB.BytesReceived() >= 4; }, std::chrono::milliseconds(500)),
           "second TCP target should receive replay payload");

    service.StopReplay();
    serverA.Stop();
    serverB.Stop();
#endif
}

void TestCaptureAndReplayStopIndependently() {
    TcpProtocolService service;
    const std::string captureConfig =
        R"({"mode":"server","host":"127.0.0.1","port":0,"channel_id":11,"channel_name":"tcp-test","topic":"tcp/topic"})";

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    LoopbackTcpServer replayServer;
    replayServer.Start();
    const std::string replayConfig =
        std::string(R"({"mode":"client","host":"127.0.0.1","port":)") +
        std::to_string(replayServer.Port()) +
        "}";
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
    replayServer.Stop();
#else
    const std::string replayConfig =
        R"({"mode":"client","host":"127.0.0.1","port":9})";
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

void TestInvalidJsonFailsCleanly() {
    TcpProtocolService service;

    bool captureThrew = false;
    bool captureStarted = false;
    try {
        captureStarted = service.StartCapture("{invalid", [](recplay::PacketPtr) {});
    } catch (...) {
        captureThrew = true;
    }

    bool replayThrew = false;
    bool replayStarted = false;
    try {
        replayStarted = service.StartReplay("{invalid");
    } catch (...) {
        replayThrew = true;
    }

    Expect(!captureThrew, "invalid TCP capture JSON should not throw");
    Expect(!replayThrew, "invalid TCP replay JSON should not throw");
    Expect(!captureStarted, "invalid TCP capture JSON should fail cleanly");
    Expect(!replayStarted, "invalid TCP replay JSON should fail cleanly");
}

} // namespace

int main() {
    try {
        TestSchemaIncludesTcpFields();
        TestCaptureLifecycleAndChannels();
        TestReplayLifecycle();
        TestReplayFanoutSupportsMultipleTargets();
        TestCaptureAndReplayStopIndependently();
        TestCapturedPacketsPopulateTimestamps();
        TestServerCaptureAcceptsSequentialConnections();
        TestInvalidJsonFailsCleanly();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
