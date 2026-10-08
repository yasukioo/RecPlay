// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <functional>
#include <string>
#include <vector>

namespace recplay {

using PacketCallback = std::function<void(PacketPtr)>;

class IProtocolService {
public:
    virtual ~IProtocolService() = default;

    virtual std::string GetName() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual int GetPriority() const = 0;

    virtual bool StartCapture(const std::string& configJson, PacketCallback cb) = 0;
    virtual void StopCapture() = 0;
    virtual bool IsCapturing() const = 0;

    virtual bool StartReplay(const std::string& configJson) = 0;
    virtual bool SendPacket(PacketPtr pkt) = 0;
    virtual void StopReplay() = 0;
    virtual bool IsReplaying() const = 0;

    virtual std::string GetConfigSchema() const = 0;
    virtual std::vector<ChannelInfo> GetChannels() const = 0;
};

} // namespace recplay
