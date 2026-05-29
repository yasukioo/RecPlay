// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RpcapReader.h"
#include "RpcapWriter.h"

#if __has_include(<nlohmann/json.hpp>)
#define RECPLAY_TEST_HAS_JSON 1
#else
#define RECPLAY_TEST_HAS_JSON 0
#endif

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using recplay::ChannelInfo;
using recplay::MutablePacketPtr;
using recplay::Packet;
using recplay::PacketFlags;
using recplay::RpcapReader;
using recplay::RpcapWriter;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path MakeTempPath(const std::string& stem) {
    return std::filesystem::temp_directory_path() / (stem + ".rpcp");
}

MutablePacketPtr MakePacket(uint64_t captureTime,
                            uint32_t channelId,
                            std::vector<uint8_t> payload,
                            std::string topic,
                            uint16_t flags = recplay::kFlagNone) {
    auto packet = std::make_shared<Packet>();
    packet->t_capture = captureTime;
    packet->t_origin = captureTime;
    packet->t_record = captureTime;
    packet->channel_id = channelId;
    packet->sequence = static_cast<uint32_t>(captureTime / 1'000'000ULL);
    packet->protocol_id = 1;
    packet->flags = flags;
    packet->payload = std::move(payload);
    packet->topic = std::move(topic);
    return packet;
}

void TestWriterReaderRoundTripAndSeekConsistency() {
    const auto path = MakeTempPath("recplay-rpcap-writer-reader");
    std::filesystem::remove(path);

    const std::vector<ChannelInfo> channels{{
        7,
        "udp-demo",
        "UDP",
        "demo/topic",
        {{"encoding", "raw"}}
    }};

    RpcapWriter writer;
    Expect(writer.Create(path.string(), channels, "none"), "writer should create an uncompressed rpcap file");
    Expect(writer.WritePacket(MakePacket(0, 7, {0x01, 0x02}, "demo/topic", recplay::kFlagKeyframe)),
           "writer should accept the first packet");
    Expect(writer.WritePacket(MakePacket(1'000'000'000ULL, 7, {0x03, 0x04}, "demo/topic")),
           "writer should accept the second packet");
    Expect(writer.WritePacket(MakePacket(2'000'000'000ULL, 7, {0x05, 0x06}, "demo/topic")),
           "writer should accept the third packet");
    Expect(writer.Finalize(), "writer should finalize the rpcap file");

    RpcapReader reader;
    Expect(reader.Open(path.string()), "reader should open the finalized rpcap file");

    const auto header = reader.GetHeader();
    Expect(header.total_packets == 3, "header should track the written packet count");
    Expect(header.duration_ns == 2'000'000'000ULL, "header should report duration from first to last packet");

    const auto read_channels = reader.GetChannels();
#if RECPLAY_TEST_HAS_JSON
    Expect(read_channels.size() == 1, "reader should recover schema channels when JSON support is available");
    Expect(read_channels.front().name == "udp-demo", "channel name should round-trip through schema");
    Expect(read_channels.front().metadata.at("encoding") == "raw",
           "channel metadata should round-trip through schema");
#else
    Expect(read_channels.empty(),
           "without JSON support the reader should still open files even though schema decoding is unavailable");
#endif

    const auto keyframes = reader.GetKeyframeTimestamps();
    Expect(keyframes == std::vector<uint64_t>({0, 1'000'000'000ULL, 2'000'000'000ULL}),
           "reader should expose keyframe timestamps written by RpcapWriter");

    const auto first = reader.ReadNext();
    const auto second = reader.ReadNext();
    const auto third = reader.ReadNext();
    Expect(first != nullptr && first->payload == std::vector<uint8_t>({0x01, 0x02}),
           "first packet payload should round-trip");
    Expect(second != nullptr && second->payload == std::vector<uint8_t>({0x03, 0x04}),
           "second packet payload should round-trip");
    Expect(third != nullptr && third->payload == std::vector<uint8_t>({0x05, 0x06}),
           "third packet payload should round-trip");
    Expect(reader.ReadNext() == nullptr, "reader should return nullptr after the final packet");
    reader.Close();

    Expect(reader.Open(path.string()), "reader should reopen the rpcap file for seek verification");
    Expect(reader.SeekTo(1'500'000'000ULL), "seek should resolve a packet at or after the requested timestamp");
    const auto seeked = reader.ReadNext();
    Expect(seeked != nullptr && seeked->t_capture == 2'000'000'000ULL,
           "seek should land on the first packet at or after the requested timestamp");
    reader.Close();

    std::filesystem::remove(path);
}

} // namespace

int main() {
    try {
        TestWriterReaderRoundTripAndSeekConsistency();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
