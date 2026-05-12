// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RpcapReader.h"

namespace recplay {

bool RpcapReader::Open(const std::string& path) {
    path_ = path;
    open_ = true;
    return true;
}

void RpcapReader::Close() {
    open_ = false;
}

bool RpcapReader::SeekTo(uint64_t timestamp_ns) {
    (void)timestamp_ns;
    return open_;
}

PacketPtr RpcapReader::ReadNext() {
    return nullptr;
}

bool RpcapReader::HasMore() const {
    return open_;
}

RpcapHeader RpcapReader::GetHeader() const {
    return header_;
}

std::vector<ChannelInfo> RpcapReader::GetChannels() const {
    return channels_;
}

} // namespace recplay
