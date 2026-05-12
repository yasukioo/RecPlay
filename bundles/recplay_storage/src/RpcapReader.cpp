// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RpcapReader.h"

#include "IoBackendFactory.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

namespace recplay {

namespace {

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
    value.resize(size);
    if (size == 0) {
        return true;
    }
    const auto read = backend->Read(value.data(), size);
    return read == static_cast<IoSize>(size);
}

} // namespace

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

    std::vector<uint8_t> schema_payload(schema_size);
    if (schema_size > 0 && !ReadBytes(schema_payload.data(), schema_payload.size())) {
        return false;
    }

#if RECPLAY_HAS_JSON
    if (!schema_payload.empty()) {
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

    std::vector<uint8_t> payload(payload_size);
    if (payload_size > 0 && !ReadBytes(payload.data(), payload.size())) {
        has_more_ = false;
        return false;
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
