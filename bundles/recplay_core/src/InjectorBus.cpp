// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "InjectorBus.h"

#include <algorithm>
#include <fstream>
#include <utility>

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
        rule.drop = item.value("drop", false);
        if (item.contains("replace")) {
            rule.replace = item.at("replace").get<std::vector<uint8_t>>();
        }
        if (!rule.drop && rule.replace.empty()) {
            return false;
        }
        parsed_rules.push_back(std::move(rule));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    static_rules_ = std::move(parsed_rules);
    return true;
#else
    (void)input;
    return false;
#endif
}

bool InjectorBus::LoadLuaScript(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    lua_scripts_.push_back(path);
    return true;
}

bool InjectorBus::LoadJsScript(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    js_scripts_.push_back(path);
    return true;
}

PacketPtr InjectorBus::Process(PacketPtr pkt) {
    if (!pkt) {
        return nullptr;
    }

    std::vector<StaticRule> static_rules;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        static_rules = static_rules_;
    }

    bool mutated = false;
    MutablePacketPtr mutable_packet = std::make_shared<Packet>(*pkt);
    for (const auto& rule : static_rules) {
        if (mutable_packet->channel_id != rule.channel_id) {
            continue;
        }
        if (rule.drop) {
            return nullptr;
        }
        if (rule.offset >= mutable_packet->payload.size()) {
            continue;
        }
        for (size_t i = 0; i < rule.replace.size() && (rule.offset + i) < mutable_packet->payload.size(); ++i) {
            mutable_packet->payload[rule.offset + i] = rule.replace[i];
            mutated = true;
        }
    }

    if (mutated) {
        return mutable_packet;
    }

    return pkt;
}

void InjectorBus::ClearRules() {
    std::lock_guard<std::mutex> lock(mutex_);
    static_rules_.clear();
    lua_scripts_.clear();
    js_scripts_.clear();
}

bool InjectorBus::HasActiveRules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !static_rules_.empty() || !lua_scripts_.empty() || !js_scripts_.empty();
}

} // namespace recplay
