// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StorageService.h"
#include "UdpProtocolService.h"

#if __has_include(<boost/asio.hpp>)
#include <boost/asio.hpp>
#define RECPLAY_TEST_HAS_BOOST_ASIO 1
#else
#define RECPLAY_TEST_HAS_BOOST_ASIO 0
#endif

#if __has_include(<nlohmann/json.hpp>)
#define RECPLAY_TEST_HAS_JSON 1
#else
#define RECPLAY_TEST_HAS_JSON 0
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef CreateFile
#undef CreateFile
#endif

namespace {

using recplay::ChannelInfo;
using recplay::PacketPtr;
using recplay::StorageService;
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

std::filesystem::path MakeTempPath(const std::string& stem) {
    return std::filesystem::temp_directory_path() / (stem + ".rpcp");
}

#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
unsigned short ReserveUdpPort() {
    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket(io_context);
    socket.open(boost::asio::ip::udp::v4());
    socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    return socket.local_endpoint().port();
}

void DrainReceivedPayloads(boost::asio::ip::udp::socket& socket,
                           std::vector<std::vector<uint8_t>>& output) {
    for (;;) {
        std::array<uint8_t, 1024> buffer{};
        boost::asio::ip::udp::endpoint sender;
        boost::system::error_code ec;
        const auto bytes =
            socket.receive_from(boost::asio::buffer(buffer), sender, 0, ec);
        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            return;
        }
        Expect(!ec, "receiving replayed UDP packet should succeed: " + ec.message());
        output.emplace_back(buffer.begin(), buffer.begin() + bytes);
    }
}
#endif

void TestUdpRecordPlaybackRoundTrip() {
#if RECPLAY_TEST_HAS_BOOST_ASIO && RECPLAY_TEST_HAS_JSON
    const auto path = MakeTempPath("recplay-record-playback");
    std::filesystem::remove(path);

    const unsigned short capture_port = ReserveUdpPort();
    const unsigned short replay_port = ReserveUdpPort();
    const std::vector<std::vector<uint8_t>> expected_payloads = {
        {0x10, 0x20, 0x30, 0x40},
        {0x50, 0x60, 0x70},
    };

    StorageService writer;
    const std::vector<ChannelInfo> channels{{
        7,
        "udp-integration",
        "UDP",
        "demo/topic",
        {
            {"address", "127.0.0.1"},
            {"port", std::to_string(capture_port)}
        }
    }};
    Expect(writer.CreateFile(path.string(), channels, "none"),
           "recording storage should create an rpcap file");

    UdpProtocolService capture_service;
    std::atomic<size_t> captured_count{0};
    std::atomic<bool> write_failed{false};
    std::mutex capture_mutex;
    std::vector<std::vector<uint8_t>> captured_payloads;
    const std::string capture_config =
        std::string(R"({"address":"127.0.0.1","interface":"127.0.0.1","port":)") +
        std::to_string(capture_port) +
        R"(,"channel_id":7,"channel_name":"udp-integration","topic":"demo/topic"})";

    Expect(capture_service.StartCapture(capture_config, [&](PacketPtr packet) {
               {
                   std::lock_guard<std::mutex> lock(capture_mutex);
                   captured_payloads.push_back(packet->payload);
               }
               if (!writer.WritePacket(packet)) {
                   write_failed.store(true, std::memory_order_release);
               }
               captured_count.fetch_add(1, std::memory_order_acq_rel);
           }),
           "UDP capture should start for integration recording");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    boost::asio::io_context sender_context;
    boost::asio::ip::udp::socket sender(sender_context);
    sender.open(boost::asio::ip::udp::v4());
    for (const auto& payload : expected_payloads) {
        sender.send_to(
            boost::asio::buffer(payload),
            boost::asio::ip::udp::endpoint(
                boost::asio::ip::make_address("127.0.0.1"),
                capture_port));
    }

    Expect(WaitUntil([&] {
               return write_failed.load(std::memory_order_acquire) ||
                   captured_count.load(std::memory_order_acquire) == expected_payloads.size();
           }, std::chrono::milliseconds(1000)),
           "UDP capture callback should receive every test payload");
    Expect(!write_failed.load(std::memory_order_acquire),
           "captured packets should write to storage successfully");

    capture_service.StopCapture();
    Expect(writer.FinalizeFile(), "recording storage should finalize the rpcap file");

    StorageService reader;
    Expect(reader.OpenFile(path.string()), "reader should reopen the recorded rpcap file");
    const auto read_channels = reader.GetChannels();
    Expect(read_channels.size() == 1, "recorded file should expose the captured channel schema");
    Expect(read_channels.front().topic == "demo/topic", "captured channel topic should round-trip");

    std::vector<std::vector<uint8_t>> file_payloads;
    while (reader.HasMore()) {
        const auto packet = reader.ReadNext();
        if (!packet) {
            break;
        }
        file_payloads.push_back(packet->payload);
    }
    Expect(file_payloads == expected_payloads, "recorded file should preserve captured UDP payload order");
    reader.CloseFile();

    boost::asio::io_context receiver_context;
    boost::asio::ip::udp::socket receiver(
        receiver_context,
        boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address("127.0.0.1"),
            replay_port));
    receiver.non_blocking(true);

    UdpProtocolService replay_service;
    const std::string replay_config =
        std::string(R"({"address":"127.0.0.1","interface":"127.0.0.1","port":)") +
        std::to_string(replay_port) + "}";
    Expect(replay_service.StartReplay(replay_config),
           "UDP replay should start for integration playback");

    Expect(reader.OpenFile(path.string()), "reader should reopen the file for replay");
    while (reader.HasMore()) {
        const auto packet = reader.ReadNext();
        if (!packet) {
            break;
        }
        Expect(replay_service.SendPacket(packet), "replay should send every packet back over UDP");
    }

    std::vector<std::vector<uint8_t>> replayed_payloads;
    Expect(WaitUntil([&] {
               DrainReceivedPayloads(receiver, replayed_payloads);
               return replayed_payloads.size() == expected_payloads.size();
           }, std::chrono::milliseconds(1000)),
           "UDP receiver should observe every replayed payload");
    Expect(replayed_payloads == expected_payloads,
           "replayed UDP payloads should match the original capture payloads");

    replay_service.StopReplay();
    reader.CloseFile();
    std::filesystem::remove(path);
#endif
}

} // namespace

int main() {
    try {
        TestUdpRecordPlaybackRoundTrip();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
