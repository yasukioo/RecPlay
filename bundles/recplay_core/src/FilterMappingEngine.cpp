// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "FilterMappingEngine.h"

#include <fstream>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#endif

namespace recplay {

bool FilterMappingEngine::LoadMapping(const std::string& jsonPath) {
    std::ifstream input(jsonPath);
    if (!input.is_open()) {
        return false;
    }

#if __has_include(<nlohmann/json.hpp>)
    nlohmann::json document;
    input >> document;

    const auto& mappings_json = document.is_array() ? document : document.at("mappings");
    std::map<uint32_t, Mapping> parsed_mappings;
    for (const auto& item : mappings_json) {
        Mapping mapping;
        mapping.source_channel = item.at("source_channel").get<uint32_t>();
        mapping.target_channel = item.at("target_channel").get<uint32_t>();
        mapping.target_topic = item.value("target_topic", std::string{});
        parsed_mappings[mapping.source_channel] = std::move(mapping);
    }

    mappings_ = std::move(parsed_mappings);
    return true;
#else
    (void)input;
    return false;
#endif
}

PacketPtr FilterMappingEngine::ApplyMapping(PacketPtr pkt) const {
    if (!pkt) {
        return nullptr;
    }

    auto mutable_packet = std::make_shared<Packet>(*pkt);
    const auto it = mappings_.find(mutable_packet->channel_id);
    if (it != mappings_.end()) {
        mutable_packet->channel_id = it->second.target_channel;
        if (!it->second.target_topic.empty()) {
            mutable_packet->topic = it->second.target_topic;
        }
    }
    if (!enabled_channels_.empty() && !IsChannelEnabled(mutable_packet->channel_id)) {
        return nullptr;
    }
    return mutable_packet;
}

void FilterMappingEngine::SetChannelFilter(std::set<uint32_t> enabledChannels) {
    enabled_channels_ = std::move(enabledChannels);
}

bool FilterMappingEngine::IsChannelEnabled(uint32_t channelId) const {
    return enabled_channels_.empty() || enabled_channels_.count(channelId) > 0;
}

} // namespace recplay
