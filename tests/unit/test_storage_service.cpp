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
#include <limits>
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

MutablePacketPtr MakePacketAt(uint64_t captureTime, uint16_t flags = recplay::kFlagNone) {
    auto packet = std::make_shared<Packet>();
    packet->t_capture = captureTime;
    packet->t_origin = captureTime;
    packet->t_record = captureTime;
    packet->channel_id = 7;
    packet->sequence = 11;
    packet->protocol_id = 2;
    packet->flags = flags;
    return packet;
}

void WriteRawFile(const std::filesystem::path& path, const recplay::RpcapHeader& header) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    Expect(file.is_open(), "should open rpcap file for write");
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    Expect(file.good(), "should write rpcap header");
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

void TestOversizedSchemaIsRejected() {
    const auto path = MakeTempPath("recplay-storage-oversized-schema");
    std::filesystem::remove(path);

    recplay::RpcapHeader header{};
    header.version = 1;
    header.schema_offset = sizeof(header);
    WriteRawFile(path, header);

    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        const uint64_t schema_size = std::numeric_limits<uint64_t>::max();
        file.write(reinterpret_cast<const char*>(&schema_size), sizeof(schema_size));
        Expect(file.good(), "should write oversized schema size");
    }

    StorageService reader;
    bool threw = false;
    bool opened = false;
    try {
        opened = reader.OpenFile(path.string());
    } catch (...) {
        threw = true;
    }

    Expect(!threw, "reader should reject oversized schema without throwing");
    Expect(!opened, "reader should reject oversized schema");

    std::filesystem::remove(path);
}

void TestInvalidSchemaJsonIsRejected() {
    const auto path = MakeTempPath("recplay-storage-invalid-schema");
    std::filesystem::remove(path);

    recplay::RpcapHeader header{};
    header.version = 1;
    header.schema_offset = sizeof(header);
    WriteRawFile(path, header);

    const std::string schema = "{bad json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        const uint64_t schema_size = static_cast<uint64_t>(schema.size());
        file.write(reinterpret_cast<const char*>(&schema_size), sizeof(schema_size));
        file.write(schema.data(), static_cast<std::streamsize>(schema.size()));
        Expect(file.good(), "should write invalid schema payload");
    }

    StorageService reader;
    bool threw = false;
    bool opened = false;
    try {
        opened = reader.OpenFile(path.string());
    } catch (...) {
        threw = true;
    }

    Expect(!threw, "reader should reject invalid schema JSON without throwing");
    Expect(!opened, "reader should reject invalid schema JSON");

    std::filesystem::remove(path);
}

void TestOversizedFooterCountIsRejected() {
    const auto path = MakeTempPath("recplay-storage-oversized-footer");
    std::filesystem::remove(path);

    recplay::RpcapHeader header{};
    header.version = 1;
    header.schema_offset = sizeof(header);
    const std::string schema = "[]";
    header.first_chunk_offset = header.schema_offset + sizeof(uint64_t) + schema.size();
    header.index_footer_offset = header.first_chunk_offset;
    WriteRawFile(path, header);

    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        const uint64_t schema_size = static_cast<uint64_t>(schema.size());
        file.write(reinterpret_cast<const char*>(&schema_size), sizeof(schema_size));
        file.write(schema.data(), static_cast<std::streamsize>(schema.size()));
        const uint64_t footer_count = std::numeric_limits<uint64_t>::max();
        file.write(reinterpret_cast<const char*>(&footer_count), sizeof(footer_count));
        Expect(file.good(), "should write oversized footer count");
    }

    StorageService reader;
    bool threw = false;
    bool opened = false;
    try {
        opened = reader.OpenFile(path.string());
    } catch (...) {
        threw = true;
    }

    Expect(!threw, "reader should reject oversized footer count without throwing");
    Expect(!opened, "reader should reject oversized footer count");

    std::filesystem::remove(path);
}

void TestOversizedChunkPayloadIsRejected() {
    const auto path = MakeTempPath("recplay-storage-oversized-payload");
    std::filesystem::remove(path);

    recplay::RpcapHeader header{};
    header.version = 1;
    header.schema_offset = sizeof(header);
    const std::string schema = "[]";
    header.first_chunk_offset = header.schema_offset + sizeof(uint64_t) + schema.size();
    header.index_footer_offset = header.first_chunk_offset + (sizeof(uint64_t) * 3);
    WriteRawFile(path, header);

    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        const uint64_t schema_size = static_cast<uint64_t>(schema.size());
        file.write(reinterpret_cast<const char*>(&schema_size), sizeof(schema_size));
        file.write(schema.data(), static_cast<std::streamsize>(schema.size()));

        const uint64_t chunk_id = 0;
        const uint64_t packet_count = 1;
        const uint64_t payload_size = std::numeric_limits<uint64_t>::max();
        const uint64_t footer_count = 0;
        file.write(reinterpret_cast<const char*>(&chunk_id), sizeof(chunk_id));
        file.write(reinterpret_cast<const char*>(&packet_count), sizeof(packet_count));
        file.write(reinterpret_cast<const char*>(&payload_size), sizeof(payload_size));
        file.write(reinterpret_cast<const char*>(&footer_count), sizeof(footer_count));
        Expect(file.good(), "should write oversized chunk payload");
    }

    StorageService reader;
    Expect(reader.OpenFile(path.string()), "reader should open file with oversized chunk payload");

    bool threw = false;
    bool readPacket = false;
    try {
        readPacket = reader.ReadNext() != nullptr;
    } catch (...) {
        threw = true;
    }

    Expect(!threw, "reader should reject oversized chunk payload without throwing");
    Expect(!readPacket, "reader should reject oversized chunk payload");

    reader.CloseFile();
    std::filesystem::remove(path);
}

void TestKeyframeIntervalIsIndependentOfChunkCount() {
    const auto path = MakeTempPath("recplay-storage-keyframe-interval");
    std::filesystem::remove(path);

    StorageService writer;
    const std::vector<recplay::ChannelInfo> channels{{
        7,
        "demo-channel",
        "TCP",
        "demo/topic",
        {}
    }};

    Expect(writer.CreateFile(path.string(), channels, "fake"), "writer should create file");
    Expect(writer.WritePacket(MakePacketAt(0, recplay::kFlagNone)), "writer should accept first packet");
    Expect(writer.WritePacket(MakePacketAt(1'100'000'000ULL, recplay::kFlagNone)),
           "writer should accept second packet");
    Expect(writer.WritePacket(MakePacketAt(2'200'000'000ULL, recplay::kFlagNone)),
           "writer should accept third packet");
    Expect(writer.WritePacket(MakePacketAt(3'300'000'000ULL, recplay::kFlagNone)),
           "writer should accept fourth packet");
    Expect(writer.FinalizeFile(), "writer should finalize file");

    StorageService reader;
    Expect(reader.OpenFile(path.string()), "reader should open keyframe test file");
    const auto timestamps = reader.GetKeyframeTimestamps();
    Expect(timestamps.size() == 4, "keyframe interval should not depend on footer count");
    Expect(timestamps[0] == 0, "first keyframe timestamp should be zero");
    Expect(timestamps[1] == 1'100'000'000ULL, "second keyframe should start at 1.1s");
    Expect(timestamps[2] == 2'200'000'000ULL, "third keyframe should start at 2.2s");
    Expect(timestamps[3] == 3'300'000'000ULL, "fourth keyframe should start at 3.3s");

    reader.CloseFile();
    std::filesystem::remove(path);
}

void TestStorageServiceTracksReadWriteStateAcrossLifecycle() {
    const auto path = MakeTempPath("recplay-storage-state-flags");
    std::filesystem::remove(path);

    StorageService storage;
    const std::vector<recplay::ChannelInfo> channels{{
        7,
        "demo-channel",
        "TCP",
        "demo/topic",
        {}
    }};

    Expect(storage.CreateFile(path.string(), channels, "none"), "storage should create file");
    Expect(storage.IsWriting(), "storage should report writing after create");
    Expect(storage.WritePacket(MakePacket()), "storage should accept packet while writing");
    Expect(storage.FinalizeFile(), "storage should finalize file");
    Expect(!storage.IsWriting(), "storage should clear writing state after finalize");

    Expect(storage.OpenFile(path.string()), "storage should open file for reading");
    Expect(storage.IsReading(), "storage should report reading after open");
    Expect(storage.HasMore(), "storage should report remaining packets after open");
    Expect(storage.ReadNext() != nullptr, "storage should read first packet");
    storage.CloseFile();
    Expect(!storage.IsReading(), "storage should clear reading state after close");

    std::filesystem::remove(path);
}

} // namespace

int main() {
    try {
        TestRoundTripUsesConfiguredCodec();
        TestCompressedFileRequiresMatchingCodec();
        TestVersionOneFilesRemainReadableWithoutCodec();
        TestOversizedSchemaIsRejected();
        TestInvalidSchemaJsonIsRejected();
        TestOversizedFooterCountIsRejected();
        TestOversizedChunkPayloadIsRejected();
        TestKeyframeIntervalIsIndependentOfChunkCount();
        TestStorageServiceTracksReadWriteStateAcrossLifecycle();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
