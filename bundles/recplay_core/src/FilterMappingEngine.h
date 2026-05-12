// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <map>
#include <mutex>
#include <set>
#include <string>

namespace recplay {

class FilterMappingEngine {
public:
    bool LoadMapping(const std::string& jsonPath);
    PacketPtr ApplyMapping(PacketPtr pkt) const;
    void SetChannelFilter(std::set<uint32_t> enabledChannels);
    bool IsChannelEnabled(uint32_t channelId) const;

private:
    struct Mapping {
        uint32_t source_channel = 0;
        uint32_t target_channel = 0;
        std::string target_topic;
    };

    mutable std::mutex mutex_;
    std::map<uint32_t, Mapping> mappings_;
    std::set<uint32_t> enabled_channels_;
};

} // namespace recplay
