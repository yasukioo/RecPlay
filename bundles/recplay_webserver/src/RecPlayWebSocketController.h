// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <memory>
#include <string_view>

namespace recplay {

class WebSocketServer;

class RecPlayWebSocketController {
public:
    static void SetServer(std::shared_ptr<WebSocketServer> server);
    static void Broadcast(std::string_view message);

private:
    static std::shared_ptr<WebSocketServer> server_;
};

} // namespace recplay
