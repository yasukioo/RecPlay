// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IInjectorService.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace recplay {

class InjectorBus final : public IInjectorService {
public:
    bool LoadStaticRules(const std::string& path) override;
    bool LoadLuaScript(const std::string& path) override;
    bool LoadJsScript(const std::string& path) override;
    PacketPtr Process(PacketPtr pkt) override;
    void ClearRules() override;
    bool HasActiveRules() const override;

private:
    struct StaticRule {
        uint32_t channel_id = 0;
        size_t offset = 0;
        bool drop = false;
        std::vector<uint8_t> replace;
    };

    mutable std::mutex mutex_;
    std::vector<StaticRule> static_rules_;
    std::vector<std::string> lua_scripts_;
    std::vector<std::string> js_scripts_;
};

} // namespace recplay
