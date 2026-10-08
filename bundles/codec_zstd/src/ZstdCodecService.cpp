// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "ZstdCodecService.h"

#if __has_include(<zstd.h>)
#include <zstd.h>
#define RECPLAY_HAS_ZSTD 1
#else
#define RECPLAY_HAS_ZSTD 0
#endif

namespace recplay {

std::string ZstdCodecService::GetName() const { return "zstd"; }

std::vector<uint8_t> ZstdCodecService::Compress(const uint8_t* data, size_t len, int level) {
    if (data == nullptr || len == 0) {
        return {};
    }

#if RECPLAY_HAS_ZSTD
    std::vector<uint8_t> compressed(ZSTD_compressBound(len));
    const size_t written = ZSTD_compress(compressed.data(), compressed.size(), data, len, level);
    if (ZSTD_isError(written) != 0U) {
        return {};
    }
    compressed.resize(written);
    return compressed;
#else
    (void)level;
    return std::vector<uint8_t>(data, data + len);
#endif
}

std::vector<uint8_t> ZstdCodecService::Decompress(const uint8_t* data, size_t len, size_t originalSize) {
    if (data == nullptr || len == 0) {
        return {};
    }

#if RECPLAY_HAS_ZSTD
    std::vector<uint8_t> decompressed(originalSize);
    const size_t written = ZSTD_decompress(decompressed.data(), decompressed.size(), data, len);
    if (ZSTD_isError(written) != 0U) {
        return {};
    }
    decompressed.resize(written);
    return decompressed;
#else
    (void)originalSize;
    return std::vector<uint8_t>(data, data + len);
#endif
}

} // namespace recplay
