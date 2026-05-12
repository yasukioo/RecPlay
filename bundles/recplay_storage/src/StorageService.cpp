// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StorageService.h"

namespace recplay {

bool StorageService::CreateFile(const std::string& path,
                                const std::vector<ChannelInfo>& channels,
                                const std::string& codec) {
    writing_ = writer_.Create(path, channels, codec);
    return writing_;
}

bool StorageService::WritePacket(PacketPtr pkt) {
    return writing_ && writer_.WritePacket(std::move(pkt));
}

bool StorageService::FinalizeFile() {
    if (!writing_) {
        return false;
    }

    writing_ = false;
    return writer_.Finalize();
}

bool StorageService::OpenFile(const std::string& path) {
    reading_ = reader_.Open(path);
    return reading_;
}

void StorageService::CloseFile() {
    reader_.Close();
    reading_ = false;
}

RpcapHeader StorageService::GetHeader() const {
    return reader_.GetHeader();
}

std::vector<ChannelInfo> StorageService::GetChannels() const {
    return reader_.GetChannels();
}

bool StorageService::SeekTo(uint64_t timestamp_ns) {
    return reading_ && reader_.SeekTo(timestamp_ns);
}

PacketPtr StorageService::ReadNext() {
    if (!reading_) {
        return nullptr;
    }
    return reader_.ReadNext();
}

bool StorageService::HasMore() const {
    return reading_ && reader_.HasMore();
}

std::vector<uint64_t> StorageService::GetKeyframeTimestamps() const {
    return reader_.GetKeyframeTimestamps();
}

bool StorageService::IsWriting() const {
    return writing_;
}

bool StorageService::IsReading() const {
    return reading_;
}

} // namespace recplay
