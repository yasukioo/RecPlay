// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <string>
#include <vector>

namespace recplay {

class RpcapReader final {
public:
    bool Open(const std::string& path);
    void Close();
    bool SeekTo(uint64_t timestamp_ns);
    PacketPtr ReadNext();
    bool HasMore() const;
    RpcapHeader GetHeader() const;
    std::vector<ChannelInfo> GetChannels() const;

private:
    std::string path_;
    bool open_ = false;
    RpcapHeader header_;
    std::vector<ChannelInfo> channels_;
};

} // namespace recplay
