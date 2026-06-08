// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StorageService.h"

#include <algorithm>
#include <cstdint>

namespace recplay {

void StorageService::SetCodecService(ICodecService* codec) {
    writer_.SetCodecService(codec);
    reader_.SetCodecService(codec);
}

bool StorageService::CreateFile(const std::string& path,
                                const std::vector<ChannelInfo>& channels,
                                const std::string& codec) {
    const bool created = writer_.Create(path, channels, codec);
    writing_.store(created, std::memory_order_release);
    return created;
}

bool StorageService::WritePacket(PacketPtr pkt) {
    return writing_.load(std::memory_order_acquire) && writer_.WritePacket(std::move(pkt));
}

bool StorageService::FinalizeFile() {
    if (!writing_.load(std::memory_order_acquire)) {
        return false;
    }

    writing_.store(false, std::memory_order_release);
    return writer_.Finalize();
}

bool StorageService::OpenFile(const std::string& path) {
    const bool opened = reader_.Open(path);
    reading_.store(opened, std::memory_order_release);
    if (opened) {
        std::lock_guard<std::mutex> lock(path_mutex_);
        current_path_ = path;
    }
    return opened;
}

void StorageService::CloseFile() {
    reader_.Close();
    reading_.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lock(path_mutex_);
    current_path_.clear();
}

RpcapHeader StorageService::GetHeader() const {
    return reader_.GetHeader();
}

std::vector<ChannelInfo> StorageService::GetChannels() const {
    return reader_.GetChannels();
}

bool StorageService::SeekTo(uint64_t timestamp_ns) {
    return reading_.load(std::memory_order_acquire) && reader_.SeekTo(timestamp_ns);
}

PacketPtr StorageService::ReadNext() {
    if (!reading_.load(std::memory_order_acquire)) {
        return nullptr;
    }
    return reader_.ReadNext();
}

bool StorageService::HasMore() const {
    return reading_.load(std::memory_order_acquire) && reader_.HasMore();
}

std::vector<uint64_t> StorageService::GetKeyframeTimestamps() const {
    return reader_.GetKeyframeTimestamps();
}

std::optional<RecordingFileInfo> StorageService::ProbeFile(const std::string& path) const {
    // Use a transient reader so probing never disturbs reader_ (which may be
    // mid-playback). Only header/schema/footer are read — no codec needed.
    RpcapReader probe;
    if (!probe.Open(path)) {
        return std::nullopt;
    }
    const auto header = probe.GetHeader();
    const auto channels = probe.GetChannels();
    probe.Close();

    RecordingFileInfo info;
    info.duration_ns = header.duration_ns;
    info.total_packets = header.total_packets;
    info.creation_time = header.creation_time;
    info.channel_count = static_cast<uint32_t>(channels.size());
    return info;
}

std::vector<uint64_t> StorageService::GetDensity(uint32_t buckets) const {
    if (buckets == 0) {
        return {};
    }

    std::string path;
    {
        std::lock_guard<std::mutex> lock(path_mutex_);
        path = current_path_;
    }
    if (path.empty()) {
        return {};
    }

    // Transient reader so the live playback reader_ position is untouched.
    RpcapReader probe;
    if (!probe.Open(path)) {
        return {};
    }
    const uint64_t duration = probe.GetHeader().duration_ns;
    const auto chunkCounts = probe.GetChunkPacketCounts();
    probe.Close();

    std::vector<uint64_t> result(buckets, 0);
    if (duration == 0 || chunkCounts.empty()) {
        return result;
    }

    const double bucketNs = static_cast<double>(duration) / static_cast<double>(buckets);
    const size_t lastBucket = buckets - 1;
    for (size_t i = 0; i < chunkCounts.size(); ++i) {
        const uint64_t count = chunkCounts[i].second;
        if (count == 0) {
            continue;
        }
        const uint64_t startTs = std::min(chunkCounts[i].first, duration);
        const uint64_t endTs =
            std::min((i + 1 < chunkCounts.size()) ? chunkCounts[i + 1].first : duration, duration);

        size_t firstBucket = std::min(lastBucket, static_cast<size_t>(static_cast<double>(startTs) / bucketNs));
        const uint64_t spanEnd = (endTs > startTs) ? (endTs - 1) : startTs;
        size_t finalBucket = std::min(lastBucket, static_cast<size_t>(static_cast<double>(spanEnd) / bucketNs));
        if (finalBucket < firstBucket) {
            finalBucket = firstBucket;
        }

        // Spread the chunk's packet count uniformly across the buckets it covers.
        const size_t span = finalBucket - firstBucket + 1;
        const uint64_t base = count / span;
        const uint64_t remainder = count % span;
        for (size_t b = firstBucket; b <= finalBucket; ++b) {
            result[b] += base + ((b - firstBucket) < remainder ? 1 : 0);
        }
    }
    return result;
}

bool StorageService::IsWriting() const {
    return writing_.load(std::memory_order_acquire);
}

bool StorageService::IsReading() const {
    return reading_.load(std::memory_order_acquire);
}

} // namespace recplay
