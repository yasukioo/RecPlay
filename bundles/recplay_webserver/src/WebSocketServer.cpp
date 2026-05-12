// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "WebSocketServer.h"

#ifdef _WIN32
#include <winsock2.h>
#include <cstdint>
inline std::uint64_t htonll(std::uint64_t value) {
    const auto high = htonl(static_cast<u_long>(value >> 32));
    const auto low = htonl(static_cast<u_long>(value & 0xffffffffULL));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}
#endif

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define RECPLAY_HAS_DROGON 1
#else
#define RECPLAY_HAS_DROGON 0
#endif

namespace recplay {

bool WebSocketServer::Start() {
#if RECPLAY_HAS_DROGON
    (void)drogon::app().getLoop();
#endif
    running_ = true;
    return true;
}

void WebSocketServer::Stop() {
    running_ = false;
}

bool WebSocketServer::IsRunning() const {
    return running_;
}

} // namespace recplay
