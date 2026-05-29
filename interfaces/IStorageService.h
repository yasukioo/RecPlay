// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <string>
#include <vector>

#ifdef CreateFile
#undef CreateFile
#endif

namespace recplay {

class IStorageService {
public:
    virtual ~IStorageService() = default;

    virtual bool CreateFile(const std::string& path,
                            const std::vector<ChannelInfo>& channels,
                            const std::string& codec = "zstd") = 0;
    virtual bool WritePacket(PacketPtr pkt) = 0;
    virtual bool FinalizeFile() = 0;

    virtual bool OpenFile(const std::string& path) = 0;
    virtual void CloseFile() = 0;
    virtual RpcapHeader GetHeader() const = 0;
    virtual std::vector<ChannelInfo> GetChannels() const = 0;

    virtual bool SeekTo(uint64_t timestamp_ns) = 0;
    virtual PacketPtr ReadNext() = 0;
    virtual bool HasMore() const = 0;
    virtual std::vector<uint64_t> GetKeyframeTimestamps() const = 0;

    virtual bool IsWriting() const = 0;
    virtual bool IsReading() const = 0;
};

} // namespace recplay
