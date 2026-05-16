// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace recplay {

class ISessionService;
class IStatsService;

class WebSocketServer {
public:
    WebSocketServer(ISessionService* session = nullptr, IStatsService* stats = nullptr);

    bool Start();
    void Stop();
    bool IsRunning() const;
    void SetSessionService(ISessionService* session);
    void SetStatsService(IStatsService* stats);
    void Broadcast(const std::string& message);
    std::vector<std::string> GetBroadcastMessages() const;
    void ClearBroadcastMessages();

private:
    void BindSubscriptions();
    void HandleStatsUpdate(const StatsSnapshot& snapshot);
    void HandleStateChanged(SessionState oldState, SessionState newState);

    mutable std::mutex mutex_;
    bool running_ = false;
    std::atomic<uint64_t> session_subscription_generation_{0};
    std::atomic<uint64_t> stats_subscription_generation_{0};
    ISessionService* session_ = nullptr;
    IStatsService* stats_ = nullptr;
    std::vector<std::string> broadcast_messages_;
};

} // namespace recplay
