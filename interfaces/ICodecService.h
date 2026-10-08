// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace recplay {

class ICodecService {
public:
    virtual ~ICodecService() = default;

    virtual std::string GetName() const = 0;

    virtual std::vector<uint8_t> Compress(const uint8_t* data, size_t len, int level = 3) = 0;
    virtual std::vector<uint8_t> Decompress(const uint8_t* data, size_t len, size_t originalSize) = 0;
};

} // namespace recplay
