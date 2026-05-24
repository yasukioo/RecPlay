// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RecPlayWebSocketController.h"

#include "WebSocketServer.h"

#if defined(_WIN32)
#include <WinSock2.h>
#ifndef htonll
static inline std::uint64_t recplay_htonll(std::uint64_t value) {
    const std::uint32_t high = htonl(static_cast<std::uint32_t>(value >> 32));
    const std::uint32_t low = htonl(static_cast<std::uint32_t>(value & 0xffffffffULL));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}
#define htonll recplay_htonll
#endif
#endif

#if __has_include(<drogon/WebSocketController.h>)
#include <drogon/WebSocketController.h>
#define RECPLAY_HAS_DROGON_WS 1
#else
#define RECPLAY_HAS_DROGON_WS 0
#endif

#include <mutex>
#include <string>
#include <unordered_set>

namespace recplay {

std::shared_ptr<WebSocketServer> RecPlayWebSocketController::server_;

void RecPlayWebSocketController::SetServer(std::shared_ptr<WebSocketServer> server) {
    server_ = std::move(server);
}

#if RECPLAY_HAS_DROGON_WS
namespace {

class DrogonRecPlayWebSocketController final
    : public drogon::WebSocketController<DrogonRecPlayWebSocketController> {
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                          std::string&&,
                          const drogon::WebSocketMessageType&) override {
    }

    void handleNewConnection(const drogon::HttpRequestPtr&,
                             const drogon::WebSocketConnectionPtr& connection) override {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.insert(connection);
    }

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& connection) override {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(connection);
    }

    static void BroadcastMessage(std::string_view message) {
        std::vector<drogon::WebSocketConnectionPtr> connections;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections.reserve(connections_.size());
            for (const auto& connection : connections_) {
                connections.push_back(connection);
            }
        }
        for (const auto& connection : connections) {
            if (connection) {
                connection->send(std::string(message));
            }
        }
    }

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/api/ws");
    WS_PATH_LIST_END

private:
    static std::mutex mutex_;
    static std::unordered_set<drogon::WebSocketConnectionPtr> connections_;
};

std::mutex DrogonRecPlayWebSocketController::mutex_;
std::unordered_set<drogon::WebSocketConnectionPtr> DrogonRecPlayWebSocketController::connections_;

} // namespace
#endif

void RecPlayWebSocketController::Broadcast(std::string_view message) {
#if RECPLAY_HAS_DROGON_WS
    DrogonRecPlayWebSocketController::BroadcastMessage(message);
#else
    (void)message;
#endif
}

} // namespace recplay
