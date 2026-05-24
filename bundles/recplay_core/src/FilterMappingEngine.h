// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace recplay {

class FilterMappingEngine {
public:
    struct MappingRule {
        uint32_t source_channel = 0;
        uint32_t target_channel = 0;
        std::string target_topic;
    };

    bool LoadMapping(const std::string& jsonPath);
    PacketPtr ApplyMapping(PacketPtr pkt) const;
    void SetMappings(std::vector<MappingRule> mappings);
    void SetChannelFilter(std::set<uint32_t> enabledChannels);
    bool IsChannelEnabled(uint32_t channelId) const;

private:
    mutable std::mutex mutex_;
    std::map<uint32_t, MappingRule> mappings_;
    std::set<uint32_t> enabled_channels_;
};

} // namespace recplay
