// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "ICodecService.h"

namespace recplay {

class ZstdCodecService final : public ICodecService {
public:
    std::string GetName() const override;
    std::vector<uint8_t> Compress(const uint8_t* data, size_t len, int level = 3) override;
    std::vector<uint8_t> Decompress(const uint8_t* data, size_t len, size_t originalSize) override;
};

} // namespace recplay
