// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <string>

namespace recplay {

class IInjectorService {
public:
    virtual ~IInjectorService() = default;

    virtual bool LoadStaticRules(const std::string& path) = 0;
    virtual bool LoadLuaScript(const std::string& path) = 0;
    virtual bool LoadJsScript(const std::string& path) = 0;
    virtual PacketPtr Process(PacketPtr pkt) = 0;
    virtual void ClearRules() = 0;
    virtual bool HasActiveRules() const = 0;
};

} // namespace recplay
