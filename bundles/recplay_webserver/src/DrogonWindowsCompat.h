// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstdint>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#ifndef htonll
static inline std::uint64_t htonll(std::uint64_t value) {
    const std::uint32_t high = htonl(static_cast<std::uint32_t>(value >> 32));
    const std::uint32_t low = htonl(static_cast<std::uint32_t>(value & 0xffffffffULL));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}
#endif
#endif
