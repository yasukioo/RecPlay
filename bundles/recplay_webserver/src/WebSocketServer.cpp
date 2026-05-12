// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "WebSocketServer.h"

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
