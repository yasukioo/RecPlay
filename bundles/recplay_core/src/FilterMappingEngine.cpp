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

    std::lock_guard<std::mutex> lock(mutex_);
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

    Mapping mapping;
    bool has_mapping = false;
    std::set<uint32_t> enabled_channels;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = mappings_.find(pkt->channel_id);
        if (it != mappings_.end()) {
            mapping = it->second;
            has_mapping = true;
        }
        enabled_channels = enabled_channels_;
    }

    const uint32_t effective_channel = has_mapping ? mapping.target_channel : pkt->channel_id;
    if (!enabled_channels.empty() && enabled_channels.count(effective_channel) == 0) {
        return nullptr;
    }

    const bool remap_topic = has_mapping && !mapping.target_topic.empty() && mapping.target_topic != pkt->topic;
    const bool remap_channel = has_mapping && mapping.target_channel != pkt->channel_id;
    if (!remap_channel && !remap_topic) {
        return pkt;
    }

    auto mutable_packet = std::make_shared<Packet>(*pkt);
    mutable_packet->channel_id = effective_channel;
    if (remap_topic) {
        mutable_packet->topic = mapping.target_topic;
    }
    return mutable_packet;
}

void FilterMappingEngine::SetChannelFilter(std::set<uint32_t> enabledChannels) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_channels_ = std::move(enabledChannels);
}

bool FilterMappingEngine::IsChannelEnabled(uint32_t channelId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_channels_.empty() || enabled_channels_.count(channelId) > 0;
}

} // namespace recplay
