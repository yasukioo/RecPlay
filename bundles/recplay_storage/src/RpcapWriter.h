// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IoBackend.h"
#include "Packet.h"
#include "IStorageService.h"

#include <memory>
#include <string>
#include <vector>

namespace recplay {

class ICodecService;
class RpcapWriter final {
public:
    bool Create(const std::string& path,
                const std::vector<ChannelInfo>& channels,
                const std::string& codec);
    bool WritePacket(PacketPtr pkt);
    bool Finalize();

private:
    std::unique_ptr<IoBackend> backend_;
    std::string codec_;
    std::vector<ChannelInfo> channels_;
    std::vector<PacketPtr> chunk_;
};

} // namespace recplay
