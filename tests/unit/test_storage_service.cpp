// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "ICodecService.h"
#include "StorageService.h"

#include <array>
#include <fstream>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using recplay::ICodecService;
using recplay::MutablePacketPtr;
using recplay::Packet;
using recplay::StorageService;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeCodecService final : public ICodecService {
public:
    std::string GetName() const override { return "fake"; }

    std::vector<uint8_t> Compress(const uint8_t* data, size_t len, int level) override {
        (void)level;
        ++compress_calls;
        std::vector<uint8_t> encoded(data, data + len);
        for (auto& byte : encoded) {
            byte ^= 0x5a;
        }
        return encoded;
    }

    std::vector<uint8_t> Decompress(const uint8_t* data, size_t len, size_t originalSize) override {
        ++decompress_calls;
        std::vector<uint8_t> decoded(data, data + len);
        for (auto& byte : decoded) {
            byte ^= 0x5a;
        }
        Expect(decoded.size() == originalSize, "decoded payload size should match original size");
        return decoded;
    }

    size_t compress_calls = 0;
    size_t decompress_calls = 0;
};

std::filesystem::path MakeTempPath(const std::string& stem) {
    return std::filesystem::temp_directory_path() / (stem + ".rpcp");
}

MutablePacketPtr MakePacket() {
    auto packet = std::make_shared<Packet>();
    packet->t_capture = 100;
    packet->t_origin = 95;
    packet->t_record = 99;
    packet->channel_id = 7;
    packet->sequence = 11;
    packet->protocol_id = 2;
    packet->flags = 1;
    packet->payload = {0x10, 0x20, 0x30, 0x40, 0x50};
    packet->topic = "demo/topic";
    return packet;
}

void TestRoundTripUsesConfiguredCodec() {
    const auto path = MakeTempPath("recplay-storage-codec-roundtrip");
    std::filesystem::remove(path);

    FakeCodecService codec;
    StorageService writer;
    writer.SetCodecService(&codec);

    const std::vector<recplay::ChannelInfo> channels{{
        7,
        "demo-channel",
        "TCP",
        "demo/topic",
        {}
    }};

    Expect(writer.CreateFile(path.string(), channels, "fake"), "writer should create file");
    Expect(writer.WritePacket(MakePacket()), "writer should accept packet");
    Expect(writer.FinalizeFile(), "writer should finalize file");
    Expect(codec.compress_calls > 0, "writer should invoke codec compression");

    StorageService reader;
    reader.SetCodecService(&codec);
    Expect(reader.OpenFile(path.string()), "reader should open compressed file");

    const auto header = reader.GetHeader();
    Expect(std::string(header.codec) == "fake", "header should record active codec");

    const auto packet = reader.ReadNext();
    Expect(packet != nullptr, "reader should return packet");
    Expect(codec.decompress_calls > 0, "reader should invoke codec decompression");
    Expect(packet->t_capture == 100, "t_capture should round-trip");
    Expect(packet->t_origin == 95, "t_origin should round-trip");
    Expect(packet->t_record == 99, "t_record should round-trip");
    Expect(packet->channel_id == 7, "channel id should round-trip");
    Expect(packet->sequence == 11, "sequence should round-trip");
    Expect(packet->protocol_id == 2, "protocol id should round-trip");
    Expect(packet->flags == 1, "flags should round-trip");
    Expect(packet->payload == std::vector<uint8_t>({0x10, 0x20, 0x30, 0x40, 0x50}),
           "payload should round-trip");
    Expect(packet->topic == "demo/topic", "topic should round-trip");

    reader.CloseFile();
    std::filesystem::remove(path);
}

void TestCompressedFileRequiresMatchingCodec() {
    const auto path = MakeTempPath("recplay-storage-codec-required");
    std::filesystem::remove(path);

    FakeCodecService codec;
    StorageService writer;
    writer.SetCodecService(&codec);

    const std::vector<recplay::ChannelInfo> channels{{
        1,
        "codec-required",
        "UDP",
        "required/topic",
        {}
    }};

    Expect(writer.CreateFile(path.string(), channels, "fake"), "writer should create file");
    Expect(writer.WritePacket(MakePacket()), "writer should write packet");
    Expect(writer.FinalizeFile(), "writer should finalize file");

    StorageService readerWithoutCodec;
    Expect(!readerWithoutCodec.OpenFile(path.string()),
           "reader should reject compressed file without matching codec");

    readerWithoutCodec.SetCodecService(&codec);
    Expect(readerWithoutCodec.OpenFile(path.string()),
           "reader should open compressed file when codec is configured");

    readerWithoutCodec.CloseFile();
    std::filesystem::remove(path);
}

void TestVersionOneFilesRemainReadableWithoutCodec() {
    const auto path = MakeTempPath("recplay-storage-version-one-compat");
    std::filesystem::remove(path);

    StorageService writer;
    const std::vector<recplay::ChannelInfo> channels{{
        9,
        "legacy-raw",
        "UDP",
        "legacy/topic",
        {}
    }};

    Expect(writer.CreateFile(path.string(), channels, "fake"), "writer should create legacy-compatible raw file");
    Expect(writer.WritePacket(MakePacket()), "writer should write legacy-compatible packet");
    Expect(writer.FinalizeFile(), "writer should finalize legacy-compatible file");

    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    Expect(file.is_open(), "should reopen rpcap file for header patch");

    recplay::RpcapHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    Expect(file.good(), "should read rpcap header");
    std::memset(header.codec, 0, sizeof(header.codec));
    std::strncpy(header.codec, "fake", sizeof(header.codec) - 1);
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.close();

    StorageService reader;
    Expect(reader.OpenFile(path.string()), "reader should accept version 1 raw chunk files without codec");
    const auto packet = reader.ReadNext();
    Expect(packet != nullptr, "reader should decode version 1 raw chunk file");
    Expect(packet->topic == "demo/topic", "legacy-compatible packet should round-trip");

    reader.CloseFile();
    std::filesystem::remove(path);
}

} // namespace

int main() {
    try {
        TestRoundTripUsesConfiguredCodec();
        TestCompressedFileRequiresMatchingCodec();
        TestVersionOneFilesRemainReadableWithoutCodec();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
