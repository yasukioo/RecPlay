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
#include <mutex>
#include <optional>
#include <vector>
#include <thread>
#include <stdexcept>
#include <string>

namespace {

using recplay::UdpProtocolService;

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
class LoopbackUdpReceiver {
public:
    LoopbackUdpReceiver()
        : socket_(
            io_context_,
            boost::asio::ip::udp::endpoint(
                boost::asio::ip::make_address("127.0.0.1"),
                0)) {}

    ~LoopbackUdpReceiver() { Stop(); }

    unsigned short Port() const {
        return socket_.local_endpoint().port();
    }

    void Start() {
        thread_ = std::thread([this] {
            std::array<uint8_t, 256> buffer{};
            boost::asio::ip::udp::endpoint remote;
            boost::system::error_code ec;
            const auto bytesRead = socket_.receive_from(boost::asio::buffer(buffer), remote, 0, ec);
            if (!ec && bytesRead > 0) {
                std::lock_guard<std::mutex> lock(mutex_);
                payload_.assign(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(bytesRead));
            }
        });
    }

    void Stop() {
        boost::system::error_code ec;
        socket_.cancel(ec);
        socket_.close(ec);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    std::vector<uint8_t> Payload() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return payload_;
    }

private:
    mutable std::mutex mutex_;
    boost::asio::io_context io_context_;
    boost::asio::ip::udp::socket socket_;
    std::thread thread_;
    std::vector<uint8_t> payload_;
};
#endif

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

void TestReplayFanoutSupportsMultipleTargets() {
#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    UdpProtocolService service;
    LoopbackUdpReceiver receiverA;
    LoopbackUdpReceiver receiverB;
    receiverA.Start();
    receiverB.Start();

    const std::string config =
        std::string("[") +
        R"({"address":"127.0.0.1","interface":"0.0.0.0","port":)" + std::to_string(receiverA.Port()) + "}," +
        R"({"address":"127.0.0.1","interface":"0.0.0.0","port":)" + std::to_string(receiverB.Port()) + "}]" ;

    Expect(service.StartReplay(config), "multi-target UDP replay should start");

    auto packet = std::make_shared<recplay::Packet>();
    packet->payload = {0xde, 0xad, 0xbe, 0xef};
    Expect(service.SendPacket(packet), "multi-target UDP replay should send packet to all targets");

    Expect(WaitUntil([&] { return receiverA.Payload().size() == 4; }, std::chrono::milliseconds(500)),
           "first UDP target should receive replay payload");
    Expect(WaitUntil([&] { return receiverB.Payload().size() == 4; }, std::chrono::milliseconds(500)),
           "second UDP target should receive replay payload");

    service.StopReplay();
    receiverA.Stop();
    receiverB.Stop();
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

void TestCapturedPacketsPopulateTimestamps() {
#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    UdpProtocolService service;
    const unsigned short port = 39011;
    const std::string config =
        R"({"address":"127.0.0.1","interface":"127.0.0.1","port":39011,"channel_id":5,"channel_name":"udp-ts","topic":"udp/timestamps"})";

    std::mutex mutex;
    std::optional<recplay::Packet> captured;
    Expect(service.StartCapture(config, [&](recplay::PacketPtr packet) {
        std::lock_guard<std::mutex> lock(mutex);
        captured = *packet;
    }), "capture should start for timestamp test");

    boost::asio::io_context ioContext;
    boost::asio::ip::udp::socket socket(ioContext);
    socket.open(boost::asio::ip::udp::v4());
    const std::array<uint8_t, 4> payload{{0x10, 0x20, 0x30, 0x40}};
    socket.send_to(
        boost::asio::buffer(payload),
        boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));

    Expect(WaitUntil([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return captured.has_value();
    }, std::chrono::milliseconds(500)), "expected UDP packet callback");

    recplay::Packet packet;
    {
        std::lock_guard<std::mutex> lock(mutex);
        packet = *captured;
    }

    Expect(packet.t_capture != 0, "captured UDP packet should set t_capture");
    Expect(packet.t_origin != 0, "captured UDP packet should set t_origin");
    Expect(packet.t_record != 0, "captured UDP packet should set t_record");

    service.StopCapture();
#endif
}

void TestInvalidJsonFailsCleanly() {
    UdpProtocolService service;

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

    Expect(!captureThrew, "invalid UDP capture JSON should not throw");
    Expect(!replayThrew, "invalid UDP replay JSON should not throw");
    Expect(!captureStarted, "invalid UDP capture JSON should fail cleanly");
    Expect(!replayStarted, "invalid UDP replay JSON should fail cleanly");
}

} // namespace

int main() {
    try {
        TestSchemaIncludesUdpFields();
        TestCaptureLifecycleAndChannels();
        TestReplayLifecycle();
        TestReplayFanoutSupportsMultipleTargets();
        TestCaptureAndReplayStopIndependently();
        TestCapturedPacketsPopulateTimestamps();
        TestInvalidJsonFailsCleanly();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
