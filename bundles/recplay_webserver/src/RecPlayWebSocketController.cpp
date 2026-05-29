// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RecPlayWebSocketController.h"

#include "DrogonWindowsCompat.h"
#include "WebSocketServer.h"

#if __has_include(<drogon/WebSocketController.h>)
#include <drogon/WebSocketController.h>
#define RECPLAY_HAS_DROGON_WS 1
#else
#define RECPLAY_HAS_DROGON_WS 0
#endif

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_set>

namespace recplay {

std::mutex RecPlayWebSocketController::server_mutex_;
std::shared_ptr<WebSocketServer> RecPlayWebSocketController::server_;

void RecPlayWebSocketController::SetServer(std::shared_ptr<WebSocketServer> server) {
    std::lock_guard<std::mutex> lock(server_mutex_);
    server_ = std::move(server);
}

std::shared_ptr<WebSocketServer> RecPlayWebSocketController::GetServer() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    return server_;
}

#if RECPLAY_HAS_DROGON_WS
namespace {

void SendWhenReady(
    const drogon::WebSocketConnectionPtr& connection,
    std::string message) {
    if (!connection) {
        return;
    }

    auto* loop = drogon::app().getLoop();
    if (loop != nullptr) {
        loop->runAfter(
            std::chrono::milliseconds(10),
            [connection, message = std::move(message)]() mutable {
                if (connection) {
                    connection->send(std::move(message));
                }
            });
        return;
    }

    connection->send(std::move(message));
}

class DrogonRecPlayWebSocketController final
    : public drogon::WebSocketController<DrogonRecPlayWebSocketController> {
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                          std::string&&,
                          const drogon::WebSocketMessageType&) override {
    }

    void handleNewConnection(const drogon::HttpRequestPtr&,
                             const drogon::WebSocketConnectionPtr& connection) override {
        std::shared_ptr<WebSocketServer> server;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.insert(connection);
            server = RecPlayWebSocketController::GetServer();
        }

        if (!connection || !server) {
            return;
        }
        const auto backlog = server->GetBroadcastMessages();
        for (const auto& message : backlog) {
            SendWhenReady(connection, message);
        }
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
                SendWhenReady(connection, std::string(message));
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
