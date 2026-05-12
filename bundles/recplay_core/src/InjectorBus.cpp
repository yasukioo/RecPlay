// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "InjectorBus.h"

#include <fstream>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#endif

namespace recplay {

bool InjectorBus::LoadStaticRules(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

#if __has_include(<nlohmann/json.hpp>)
    nlohmann::json document;
    input >> document;

    const auto& rules_json = document.is_array() ? document : document.at("rules");
    std::vector<StaticRule> parsed_rules;
    for (const auto& item : rules_json) {
        StaticRule rule;
        rule.channel_id = item.at("channel_id").get<uint32_t>();
        rule.offset = item.at("offset").get<size_t>();
        rule.replace = item.at("replace").get<std::vector<uint8_t>>();
        parsed_rules.push_back(std::move(rule));
    }

    static_rules_ = std::move(parsed_rules);
    return true;
#else
    (void)input;
    return false;
#endif
}

bool InjectorBus::LoadLuaScript(const std::string& path) {
    lua_scripts_.push_back(path);
    return true;
}

bool InjectorBus::LoadJsScript(const std::string& path) {
    js_scripts_.push_back(path);
    return true;
}

PacketPtr InjectorBus::Process(PacketPtr pkt) {
    if (!pkt) {
        return nullptr;
    }

    MutablePacketPtr mutable_packet = std::make_shared<Packet>(*pkt);
    for (const auto& rule : static_rules_) {
        if (mutable_packet->channel_id != rule.channel_id) {
            continue;
        }
        if (rule.offset >= mutable_packet->payload.size()) {
            continue;
        }
        for (size_t i = 0; i < rule.replace.size() && (rule.offset + i) < mutable_packet->payload.size(); ++i) {
            mutable_packet->payload[rule.offset + i] = rule.replace[i];
        }
    }
    return mutable_packet;
}

void InjectorBus::ClearRules() {
    static_rules_.clear();
    lua_scripts_.clear();
    js_scripts_.clear();
}

bool InjectorBus::HasActiveRules() const {
    return !static_rules_.empty() || !lua_scripts_.empty() || !js_scripts_.empty();
}

} // namespace recplay
