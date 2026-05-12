// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IProtocolService.h"

namespace recplay {

class UdpProtocolService final : public IProtocolService {
public:
    std::string GetName() const override;
    std::string GetVersion() const override;
    int GetPriority() const override;
    bool StartCapture(const std::string& configJson, PacketCallback cb) override;
    void StopCapture() override;
    bool IsCapturing() const override;
    bool StartReplay(const std::string& configJson) override;
    bool SendPacket(PacketPtr pkt) override;
    void StopReplay() override;
    bool IsReplaying() const override;
    std::string GetConfigSchema() const override;
    std::vector<ChannelInfo> GetChannels() const override;

private:
    bool capturing_ = false;
    bool replaying_ = false;
};

} // namespace recplay
