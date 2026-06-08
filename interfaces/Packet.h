// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace recplay {

enum class ProtocolId : uint16_t {
    kUDP = 1,
    kTCP = 2,
    kDDS = 3,
    kHLA = 4,
    kSerial = 5,
};

enum PacketFlags : uint16_t {
    kFlagNone = 0x00,
    kFlagKeyframe = 0x01,
    kFlagCompressed = 0x02,
};

struct Packet {
    uint64_t t_capture = 0;
    uint64_t t_origin = 0;
    uint64_t t_record = 0;
    uint32_t channel_id = 0;
    uint32_t sequence = 0;
    uint16_t protocol_id = 0;
    uint16_t flags = 0;
    std::vector<uint8_t> payload;
    std::string topic;
    std::map<std::string, std::string> metadata;
};

using PacketPtr = std::shared_ptr<const Packet>;
using MutablePacketPtr = std::shared_ptr<Packet>;

struct ChannelInfo {
    uint32_t id = 0;
    std::string name;
    std::string protocol;
    std::string topic;
    std::map<std::string, std::string> metadata;
};

enum class SessionState {
    Idle,
    Recording,
    RecordingPaused,
    Playing,
    PlaybackPaused,
    Seeking,
    Stopped,
};

struct ProtocolStats {
    double throughput_mbps = 0.0;
    uint64_t packets = 0;
    uint64_t drops = 0;
};

struct ChannelStats {
    double throughput_mbps = 0.0;
    uint64_t packets = 0;
};

struct StatsSnapshot {
    double total_throughput_mbps = 0.0;
    uint64_t total_packets = 0;
    uint64_t total_drops = 0;
    uint64_t total_bytes = 0;        // cumulative bytes captured across all protocols
    double drop_rate = 0.0;
    double write_latency_p99_ms = 0.0;
    uint32_t ringbuf_used = 0;
    uint32_t ringbuf_capacity = 0;
    uint64_t disk_queue_bytes = 0;
    std::optional<double> cpu_usage_percent;
    std::map<std::string, ProtocolStats> per_protocol;
    std::map<uint32_t, ChannelStats> per_channel;
};

struct RpcapHeader {
    uint8_t magic[4] = {'R', 'P', 'C', 'P'};
    uint32_t version = 1;
    uint64_t schema_offset = 0;
    uint64_t first_chunk_offset = 0;
    uint64_t index_footer_offset = 0;
    uint64_t total_packets = 0;
    uint64_t duration_ns = 0;
    uint64_t creation_time = 0;
    char codec[16] = {};
};

inline const char* SessionStateToString(SessionState state) {
    switch (state) {
        case SessionState::Idle:
            return "Idle";
        case SessionState::Recording:
            return "Recording";
        case SessionState::RecordingPaused:
            return "RecordingPaused";
        case SessionState::Playing:
            return "Playing";
        case SessionState::PlaybackPaused:
            return "PlaybackPaused";
        case SessionState::Seeking:
            return "Seeking";
        case SessionState::Stopped:
            return "Stopped";
    }
    return "Unknown";
}

} // namespace recplay
