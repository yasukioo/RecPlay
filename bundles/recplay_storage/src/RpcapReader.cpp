// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RpcapReader.h"

#include "ICodecService.h"
#include "IoBackendFactory.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#if __has_include(<zstd.h>)
#include <zstd.h>
#define RECPLAY_HAS_ZSTD 1
#else
#define RECPLAY_HAS_ZSTD 0
#endif

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

namespace recplay {

namespace {

constexpr uint64_t kMaxSchemaSize = 16ULL * 1024ULL * 1024ULL;
constexpr uint64_t kMaxFooterCount = 1'000'000ULL;
constexpr uint64_t kMaxChunkPayloadSize = 128ULL * 1024ULL * 1024ULL;
constexpr uint64_t kMaxChunkPacketCount = 1'000'000ULL;
constexpr uint32_t kMaxStringSize = 16U * 1024U * 1024U;

template <typename T>
bool ReadScalar(IoBackend* backend, T& value) {
    if (backend == nullptr) {
        return false;
    }
    const auto read = backend->Read(&value, sizeof(T));
    return read == static_cast<IoSize>(sizeof(T));
}

bool ReadString(IoBackend* backend, std::string& value) {
    uint32_t size = 0;
    if (!ReadScalar(backend, size)) {
        return false;
    }
    if (size > kMaxStringSize) {
        return false;
    }
    value.resize(size);
    if (size == 0) {
        return true;
    }
    const auto read = backend->Read(value.data(), size);
    return read == static_cast<IoSize>(size);
}

bool ReadBytes(IoBackend* backend, std::vector<uint8_t>& value) {
    uint32_t size = 0;
    if (!ReadScalar(backend, size)) {
        return false;
    }
    if (size > kMaxStringSize) {
        return false;
    }
    value.resize(size);
    if (size == 0) {
        return true;
    }
    const auto read = backend->Read(value.data(), size);
    return read == static_cast<IoSize>(size);
}

std::string GetCodecName(const recplay::RpcapHeader& header) {
    const auto* begin = header.codec;
    const auto* end = std::find(begin, begin + sizeof(header.codec), '\0');
    return std::string(begin, end);
}

bool IsCompressedChunkFormat(const recplay::RpcapHeader& header) {
    return header.version >= 2 && !GetCodecName(header).empty();
}

std::vector<uint8_t> DecompressCompressedChunk(const std::string& codec_name,
                                               const uint8_t* data,
                                               size_t len,
                                               size_t original_size,
                                               ICodecService* codec_service) {
    if (codec_service != nullptr && codec_service->GetName() == codec_name) {
        return codec_service->Decompress(data, len, original_size);
    }

#if RECPLAY_HAS_ZSTD
    if (codec_name == "zstd") {
        std::vector<uint8_t> decompressed(original_size);
        const size_t written = ZSTD_decompress(decompressed.data(), decompressed.size(), data, len);
        if (ZSTD_isError(written) != 0U) {
            return {};
        }
        decompressed.resize(written);
        return decompressed;
    }
#else
    (void)codec_name;
#endif

    return {};
}

} // namespace

void RpcapReader::SetCodecService(ICodecService* codec) {
    codec_service_ = codec;
}

bool RpcapReader::Open(const std::string& path) {
    Close();

    backend_ = IoBackendFactory::Create();
    if (!backend_ || !backend_->Open(path, true)) {
        backend_.reset();
        return false;
    }

    path_ = path;
    if (!ReadBytes(&header_, sizeof(header_))) {
        Close();
        return false;
    }
    if (std::memcmp(header_.magic, "RPCP", 4) != 0) {
        Close();
        return false;
    }
    const auto codec_name = GetCodecName(header_);
    if (IsCompressedChunkFormat(header_)) {
        const bool has_matching_codec_service =
            codec_service_ != nullptr && codec_service_->GetName() == codec_name;
        const bool has_builtin_fallback =
#if RECPLAY_HAS_ZSTD
            codec_name == "zstd";
#else
            false;
#endif
        if (!has_matching_codec_service && !has_builtin_fallback) {
            Close();
            return false;
        }
    }
    if (!LoadSchema() || !LoadFooter()) {
        Close();
        return false;
    }

    next_chunk_offset_ = header_.first_chunk_offset;
    current_chunk_.clear();
    current_chunk_index_ = 0;
    open_ = true;
    has_more_ = next_chunk_offset_ < header_.index_footer_offset;
    return true;
}

void RpcapReader::Close() {
    current_chunk_.clear();
    current_chunk_index_ = 0;
    footer_entries_.clear();
    channels_.clear();
    has_more_ = false;
    open_ = false;
    next_chunk_offset_ = 0;
    if (backend_) {
        backend_->Close();
        backend_.reset();
    }
}

bool RpcapReader::SeekTo(uint64_t timestamp_ns) {
    if (!open_ || backend_ == nullptr) {
        return false;
    }

    uint64_t target_chunk_offset = header_.first_chunk_offset;
    for (const auto& entry : footer_entries_) {
        if (entry.timestamp_ns > timestamp_ns) {
            break;
        }
        target_chunk_offset = entry.chunk_offset;
    }

    current_chunk_.clear();
    current_chunk_index_ = 0;
    next_chunk_offset_ = target_chunk_offset;
    has_more_ = next_chunk_offset_ < header_.index_footer_offset;

    while (LoadNextChunk()) {
        auto it = std::find_if(
            current_chunk_.begin(),
            current_chunk_.end(),
            [timestamp_ns](const PacketPtr& pkt) {
                return pkt != nullptr && pkt->t_capture >= timestamp_ns;
            });
        if (it != current_chunk_.end()) {
            current_chunk_index_ = static_cast<size_t>(std::distance(current_chunk_.begin(), it));
            return true;
        }
        if (!has_more_) {
            break;
        }
    }

    return !current_chunk_.empty();
}

PacketPtr RpcapReader::ReadNext() {
    if (!open_) {
        return nullptr;
    }

    if (current_chunk_index_ >= current_chunk_.size()) {
        if (!LoadNextChunk()) {
            return nullptr;
        }
    }

    if (current_chunk_index_ >= current_chunk_.size()) {
        return nullptr;
    }

    return current_chunk_[current_chunk_index_++];
}

bool RpcapReader::HasMore() const {
    return open_ && (current_chunk_index_ < current_chunk_.size() || has_more_);
}

RpcapHeader RpcapReader::GetHeader() const {
    return header_;
}

std::vector<ChannelInfo> RpcapReader::GetChannels() const {
    return channels_;
}

std::vector<uint64_t> RpcapReader::GetKeyframeTimestamps() const {
    std::vector<uint64_t> timestamps;
    timestamps.reserve(footer_entries_.size());
    for (const auto& entry : footer_entries_) {
        timestamps.push_back(entry.timestamp_ns);
    }
    return timestamps;
}

std::vector<std::pair<uint64_t, uint64_t>> RpcapReader::GetChunkPacketCounts() const {
    std::vector<std::pair<uint64_t, uint64_t>> result;
    if (!open_ || backend_ == nullptr) {
        return result;
    }
    result.reserve(footer_entries_.size());
    for (const auto& entry : footer_entries_) {
        // Chunk layout: [chunk_id(8)][packet_count(8)][payload_size(8)][payload].
        // Seek past chunk_id to read packet_count without touching the payload.
        if (!backend_->Seek(static_cast<int64_t>(entry.chunk_offset + sizeof(uint64_t)))) {
            continue;
        }
        uint64_t packet_count = 0;
        if (backend_->Read(&packet_count, sizeof(packet_count)) !=
            static_cast<IoSize>(sizeof(packet_count))) {
            continue;
        }
        if (packet_count > kMaxChunkPacketCount) {
            packet_count = 0;
        }
        result.emplace_back(entry.timestamp_ns, packet_count);
    }
    return result;
}

bool RpcapReader::ReadBytes(void* data, size_t size) {
    if (backend_ == nullptr) {
        return false;
    }
    const auto read = backend_->Read(data, size);
    return read == static_cast<IoSize>(size);
}

bool RpcapReader::LoadSchema() {
    if (backend_ == nullptr || !backend_->Seek(static_cast<int64_t>(header_.schema_offset))) {
        return false;
    }

    uint64_t schema_size = 0;
    if (!ReadBytes(&schema_size, sizeof(schema_size))) {
        return false;
    }
    if (schema_size > kMaxSchemaSize) {
        return false;
    }

    std::vector<uint8_t> schema_payload(static_cast<size_t>(schema_size));
    if (schema_size > 0 && !ReadBytes(schema_payload.data(), schema_payload.size())) {
        return false;
    }

#if RECPLAY_HAS_JSON
    if (!schema_payload.empty()) {
        try {
            const auto document = nlohmann::json::parse(schema_payload.begin(), schema_payload.end());
            channels_.clear();
            for (const auto& item : document) {
                ChannelInfo channel;
                channel.id = item.value("id", 0U);
                channel.name = item.value("name", std::string{});
                channel.protocol = item.value("protocol", std::string{});
                channel.topic = item.value("topic", std::string{});
                if (item.contains("metadata")) {
                    channel.metadata = item.at("metadata").get<std::map<std::string, std::string>>();
                }
                channels_.push_back(std::move(channel));
            }
        } catch (...) {
            return false;
        }
    }
#else
    (void)schema_payload;
#endif
    return true;
}

bool RpcapReader::LoadFooter() {
    footer_entries_.clear();
    if (backend_ == nullptr || !backend_->Seek(static_cast<int64_t>(header_.index_footer_offset))) {
        return false;
    }

    uint64_t footer_count = 0;
    if (!ReadBytes(&footer_count, sizeof(footer_count))) {
        return false;
    }
    if (footer_count > kMaxFooterCount) {
        return false;
    }

    footer_entries_.resize(static_cast<size_t>(footer_count));
    for (auto& entry : footer_entries_) {
        if (!ReadBytes(&entry, sizeof(entry))) {
            return false;
        }
    }
    return true;
}

bool RpcapReader::LoadNextChunk() {
    current_chunk_.clear();
    current_chunk_index_ = 0;
    if (!open_ || backend_ == nullptr || !has_more_) {
        return false;
    }
    if (next_chunk_offset_ >= header_.index_footer_offset) {
        has_more_ = false;
        return false;
    }

    if (!backend_->Seek(static_cast<int64_t>(next_chunk_offset_))) {
        has_more_ = false;
        return false;
    }

    uint64_t chunk_id = 0;
    uint64_t packet_count = 0;
    uint64_t payload_size = 0;
    if (!ReadBytes(&chunk_id, sizeof(chunk_id)) ||
        !ReadBytes(&packet_count, sizeof(packet_count)) ||
        !ReadBytes(&payload_size, sizeof(payload_size))) {
        has_more_ = false;
        return false;
    }
    (void)chunk_id;
    if (packet_count > kMaxChunkPacketCount || payload_size > kMaxChunkPayloadSize) {
        has_more_ = false;
        return false;
    }

    std::vector<uint8_t> payload(static_cast<size_t>(payload_size));
    if (payload_size > 0 && !ReadBytes(payload.data(), payload.size())) {
        has_more_ = false;
        return false;
    }

    const auto codec_name = GetCodecName(header_);
    if (IsCompressedChunkFormat(header_)) {
        if (payload.size() < sizeof(uint64_t)) {
            has_more_ = false;
            return false;
        }

        uint64_t original_size = 0;
        std::memcpy(&original_size, payload.data(), sizeof(original_size));
        auto decompressed = DecompressCompressedChunk(
            codec_name,
            payload.data() + sizeof(original_size),
            payload.size() - sizeof(original_size),
            static_cast<size_t>(original_size),
            codec_service_);
        if (decompressed.size() != static_cast<size_t>(original_size)) {
            has_more_ = false;
            return false;
        }
        payload = std::move(decompressed);
    }

    next_chunk_offset_ += sizeof(chunk_id) + sizeof(packet_count) + sizeof(payload_size) + payload_size;
    has_more_ = next_chunk_offset_ < header_.index_footer_offset;

    size_t cursor = 0;
    current_chunk_.reserve(static_cast<size_t>(packet_count));
    for (uint64_t i = 0; i < packet_count; ++i) {
        auto packet = std::make_shared<Packet>();

        auto read_from_payload = [&](void* dst, size_t size) -> bool {
            if (cursor + size > payload.size()) {
                return false;
            }
            std::memcpy(dst, payload.data() + cursor, size);
            cursor += size;
            return true;
        };

        uint32_t payload_bytes = 0;
        uint32_t topic_bytes = 0;
        if (!read_from_payload(&packet->t_capture, sizeof(packet->t_capture)) ||
            !read_from_payload(&packet->t_origin, sizeof(packet->t_origin)) ||
            !read_from_payload(&packet->t_record, sizeof(packet->t_record)) ||
            !read_from_payload(&packet->channel_id, sizeof(packet->channel_id)) ||
            !read_from_payload(&packet->sequence, sizeof(packet->sequence)) ||
            !read_from_payload(&packet->protocol_id, sizeof(packet->protocol_id)) ||
            !read_from_payload(&packet->flags, sizeof(packet->flags)) ||
            !read_from_payload(&payload_bytes, sizeof(payload_bytes))) {
            current_chunk_.clear();
            has_more_ = false;
            return false;
        }

        packet->payload.resize(payload_bytes);
        if (payload_bytes > 0 && !read_from_payload(packet->payload.data(), payload_bytes)) {
            current_chunk_.clear();
            has_more_ = false;
            return false;
        }

        if (!read_from_payload(&topic_bytes, sizeof(topic_bytes))) {
            current_chunk_.clear();
            has_more_ = false;
            return false;
        }
        packet->topic.resize(topic_bytes);
        if (topic_bytes > 0 && !read_from_payload(packet->topic.data(), topic_bytes)) {
            current_chunk_.clear();
            has_more_ = false;
            return false;
        }

        current_chunk_.push_back(packet);
    }

    return !current_chunk_.empty();
}

} // namespace recplay
