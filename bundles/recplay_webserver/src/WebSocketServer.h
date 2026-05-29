// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <string>
#include <vector>

namespace recplay {

class ISessionService;
class IStatsService;

class WebSocketServer {
public:
    struct SharedState {
        mutable std::mutex mutex;
        bool running = false;
        std::atomic<uint64_t> session_subscription_generation{0};
        std::atomic<uint64_t> stats_subscription_generation{0};
        std::atomic<ISessionService*> session{nullptr};
        std::atomic<IStatsService*> stats{nullptr};
        std::vector<std::string> broadcast_messages;
        std::function<void(std::string_view)> transport;
    };

    WebSocketServer(ISessionService* session = nullptr, IStatsService* stats = nullptr);

    bool Start();
    void Stop();
    bool IsRunning() const;
    void SetSessionService(ISessionService* session);
    void SetStatsService(IStatsService* stats);
    void Broadcast(const std::string& message);
    std::vector<std::string> GetBroadcastMessages() const;
    void ClearBroadcastMessages();
    void SetTransport(std::function<void(std::string_view)> transport);

private:
    void BindSubscriptions();
    void HandleStatsUpdate(const StatsSnapshot& snapshot);
    void HandleStateChanged(SessionState oldState, SessionState newState);

    std::shared_ptr<SharedState> shared_state_;
};

} // namespace recplay
