// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StorageService.h"

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
    return opened;
}

void StorageService::CloseFile() {
    reader_.Close();
    reading_.store(false, std::memory_order_release);
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

bool StorageService::IsWriting() const {
    return writing_.load(std::memory_order_acquire);
}

bool StorageService::IsReading() const {
    return reading_.load(std::memory_order_acquire);
}

} // namespace recplay
