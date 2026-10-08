// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <memory>
#include <mutex>
#include <string_view>

namespace recplay {

class WebSocketServer;

class RecPlayWebSocketController {
public:
    static void SetServer(std::shared_ptr<WebSocketServer> server);
    static std::shared_ptr<WebSocketServer> GetServer();
    static void Broadcast(std::string_view message);

private:
    static std::mutex server_mutex_;
    static std::shared_ptr<WebSocketServer> server_;
};

} // namespace recplay
