// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RpcapWriter.h"

#include "IoBackendFactory.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <utility>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

namespace recplay {

namespace {

constexpr uint64_t kChunkFlushThresholdBytes = 64ULL * 1024ULL * 1024ULL;
constexpr uint64_t kKeyframeIntervalNs = 1'000'000'000ULL;

template <typename T>
void AppendScalar(std::vector<uint8_t>& buffer, const T& value) {
    const auto* begin = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), begin, begin + sizeof(T));
}

void AppendString(std::vector<uint8_t>& buffer, const std::string& value) {
    const auto size = static_cast<uint32_t>(value.size());
    AppendScalar(buffer, size);
    buffer.insert(buffer.end(), value.begin(), value.end());
}

void AppendBytes(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& value) {
    const auto size = static_cast<uint32_t>(value.size());
    AppendScalar(buffer, size);
    buffer.insert(buffer.end(), value.begin(), value.end());
}

} // namespace

bool RpcapWriter::Create(const std::string& path,
                         const std::vector<ChannelInfo>& channels,
                         const std::string& codec) {
    backend_ = IoBackendFactory::Create();
    if (!backend_ || !backend_->Open(path, false)) {
        return false;
    }

    header_ = {};
    std::memset(header_.codec, 0, sizeof(header_.codec));
    std::strncpy(header_.codec, codec.c_str(), sizeof(header_.codec) - 1);
    header_.creation_time = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    codec_ = codec;
    channels_ = channels;
    chunk_.clear();
    footer_entries_.clear();
    current_offset_ = 0;
    current_chunk_id_ = 0;
    first_packet_ts_ = 0;
    last_packet_ts_ = 0;
    packets_written_ = 0;

    if (!WriteBytes(&header_, sizeof(header_))) {
        backend_->Close();
        backend_.reset();
        return false;
    }

    header_.schema_offset = current_offset_;
    const auto schema_payload = BuildSchemaPayload();
    const auto schema_size = static_cast<uint64_t>(schema_payload.size());
    if (!WriteBytes(&schema_size, sizeof(schema_size)) ||
        !WriteBytes(schema_payload.data(), schema_payload.size())) {
        backend_->Close();
        backend_.reset();
        return false;
    }

    header_.first_chunk_offset = current_offset_;
    return true;
}

bool RpcapWriter::WritePacket(PacketPtr pkt) {
    if (!backend_ || !pkt) {
        return false;
    }

    if (packets_written_ == 0) {
        first_packet_ts_ = pkt->t_capture;
    }
    last_packet_ts_ = pkt->t_capture;
    chunk_.push_back(pkt);

    uint64_t estimated_chunk_size = 0;
    for (const auto& item : chunk_) {
        if (!item) {
            continue;
        }
        estimated_chunk_size += sizeof(item->t_capture) +
            sizeof(item->t_origin) +
            sizeof(item->t_record) +
            sizeof(item->channel_id) +
            sizeof(item->sequence) +
            sizeof(item->protocol_id) +
            sizeof(item->flags) +
            sizeof(uint32_t) + item->payload.size() +
            sizeof(uint32_t) + item->topic.size();
    }

    const bool force_keyframe =
        (pkt->flags & kFlagKeyframe) != 0 ||
        packets_written_ == 0 ||
        (pkt->t_capture >= first_packet_ts_ &&
         (pkt->t_capture - first_packet_ts_) >=
             ((footer_entries_.size() + 1) * kKeyframeIntervalNs));

    if (estimated_chunk_size >= kChunkFlushThresholdBytes || force_keyframe) {
        return FlushChunk(force_keyframe);
    }

    return true;
}

bool RpcapWriter::Finalize() {
    if (!backend_) {
        return false;
    }

    if (!chunk_.empty() && !FlushChunk(true)) {
        return false;
    }

    header_.index_footer_offset = current_offset_;
    header_.total_packets = packets_written_;
    header_.duration_ns = packets_written_ == 0 ? 0 : (last_packet_ts_ - first_packet_ts_);

    const auto footer_count = static_cast<uint64_t>(footer_entries_.size());
    if (!WriteBytes(&footer_count, sizeof(footer_count))) {
        return false;
    }
    for (const auto& entry : footer_entries_) {
        if (!WriteBytes(&entry, sizeof(entry))) {
            return false;
        }
    }

    if (!backend_->Seek(0) || !WriteBytes(&header_, sizeof(header_))) {
        return false;
    }

    backend_->Sync();
    backend_->Close();
    backend_.reset();
    return true;
}

bool RpcapWriter::FlushChunk(bool force_keyframe) {
    if (!backend_ || chunk_.empty()) {
        return true;
    }

    const auto chunk_offset = current_offset_;
    if (footer_entries_.empty() || force_keyframe) {
        footer_entries_.push_back(FooterEntry{chunk_.front()->t_capture, chunk_offset});
    }

    const auto payload = BuildChunkPayload();
    const auto payload_size = static_cast<uint64_t>(payload.size());
    const auto packet_count = static_cast<uint64_t>(chunk_.size());
    const auto chunk_id = current_chunk_id_++;

    if (!WriteBytes(&chunk_id, sizeof(chunk_id)) ||
        !WriteBytes(&packet_count, sizeof(packet_count)) ||
        !WriteBytes(&payload_size, sizeof(payload_size)) ||
        !WriteBytes(payload.data(), payload.size())) {
        return false;
    }

    packets_written_ += packet_count;
    chunk_.clear();
    return true;
}

bool RpcapWriter::WriteBytes(const void* data, size_t size) {
    if (!backend_) {
        return false;
    }
    if (size == 0) {
        return true;
    }

    const auto written = backend_->Write(data, size);
    if (written < 0 || static_cast<size_t>(written) != size) {
        return false;
    }
    current_offset_ += static_cast<uint64_t>(written);
    return true;
}

std::vector<uint8_t> RpcapWriter::BuildSchemaPayload() const {
    std::vector<uint8_t> payload;
#if RECPLAY_HAS_JSON
    nlohmann::json document = nlohmann::json::array();
    for (const auto& channel : channels_) {
        document.push_back({
            {"id", channel.id},
            {"name", channel.name},
            {"protocol", channel.protocol},
            {"topic", channel.topic},
            {"metadata", channel.metadata},
        });
    }
    const auto encoded = document.dump();
    payload.insert(payload.end(), encoded.begin(), encoded.end());
#else
    for (const auto& channel : channels_) {
        AppendScalar(payload, channel.id);
        AppendString(payload, channel.name);
        AppendString(payload, channel.protocol);
        AppendString(payload, channel.topic);
    }
#endif
    return payload;
}

std::vector<uint8_t> RpcapWriter::BuildChunkPayload() const {
    std::vector<uint8_t> payload;
    payload.reserve(chunk_.size() * 128);

    for (const auto& packet : chunk_) {
        if (!packet) {
            continue;
        }
        AppendScalar(payload, packet->t_capture);
        AppendScalar(payload, packet->t_origin);
        AppendScalar(payload, packet->t_record);
        AppendScalar(payload, packet->channel_id);
        AppendScalar(payload, packet->sequence);
        AppendScalar(payload, packet->protocol_id);
        AppendScalar(payload, packet->flags);
        AppendBytes(payload, packet->payload);
        AppendString(payload, packet->topic);
    }

    return payload;
}

} // namespace recplay
