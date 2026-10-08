// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <string>

namespace recplay {

class ISinkService {
public:
    virtual ~ISinkService() = default;

    virtual std::string GetFormat() const = 0;
    virtual std::string GetExtension() const = 0;
    virtual bool Open(const std::string& path, const std::string& configJson) = 0;
    virtual bool WritePacket(PacketPtr pkt) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
};

} // namespace recplay
