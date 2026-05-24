// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "WebSocketServer.h"

#include "IStatsService.h"
#include "ISessionService.h"

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

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define RECPLAY_HAS_DROGON 1
#else
#define RECPLAY_HAS_DROGON 0
#endif

#include <sstream>

namespace recplay {

WebSocketServer::WebSocketServer(ISessionService* session, IStatsService* stats)
    : session_(session), stats_(stats) {}

bool WebSocketServer::Start() {
#if RECPLAY_HAS_DROGON
    (void)drogon::app().getLoop();
#endif
    BindSubscriptions();
    running_ = true;
    return true;
}

void WebSocketServer::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    ++session_subscription_generation_;
    ++stats_subscription_generation_;
}

bool WebSocketServer::IsRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void WebSocketServer::SetSessionService(ISessionService* session) {
    session_ = session;
    if (running_) {
        BindSubscriptions();
    } else {
        ++session_subscription_generation_;
    }
}

void WebSocketServer::SetStatsService(IStatsService* stats) {
    stats_ = stats;
    if (running_) {
        BindSubscriptions();
    } else {
        ++stats_subscription_generation_;
    }
}

std::vector<std::string> WebSocketServer::GetBroadcastMessages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return broadcast_messages_;
}

void WebSocketServer::ClearBroadcastMessages() {
    std::lock_guard<std::mutex> lock(mutex_);
    broadcast_messages_.clear();
}

void WebSocketServer::SetTransport(std::function<void(std::string_view)> transport) {
    std::lock_guard<std::mutex> lock(mutex_);
    transport_ = std::move(transport);
}

void WebSocketServer::BindSubscriptions() {
    if (stats_ != nullptr) {
        const uint64_t generation = ++stats_subscription_generation_;
        stats_->OnUpdate([this, generation](const StatsSnapshot& snapshot) {
            if (generation != stats_subscription_generation_.load()) {
                return;
            }
            HandleStatsUpdate(snapshot);
        });
    }
    if (session_ != nullptr) {
        const uint64_t generation = ++session_subscription_generation_;
        session_->OnStateChanged([this, generation](SessionState oldState, SessionState newState) {
            if (generation != session_subscription_generation_.load()) {
                return;
            }
            HandleStateChanged(oldState, newState);
        });
    }
}

void WebSocketServer::HandleStatsUpdate(const StatsSnapshot& snapshot) {
    std::ostringstream stream;
    stream << "{\"type\":\"stats\""
           << ",\"data\":{\"total_packets\":" << snapshot.total_packets
           << ",\"total_drops\":" << snapshot.total_drops
           << ",\"total_throughput_mbps\":" << snapshot.total_throughput_mbps
           << ",\"cpu_usage_percent\":";
    if (snapshot.cpu_usage_percent.has_value()) {
        stream << *snapshot.cpu_usage_percent;
    } else {
        stream << "null";
    }
    stream << "}}";
    Broadcast(stream.str());
}

void WebSocketServer::HandleStateChanged(SessionState oldState, SessionState newState) {
    std::ostringstream stream;
    stream << "{\"type\":\"state_changed\""
           << ",\"data\":{\"old_state\":\"" << SessionStateToString(oldState)
           << "\",\"new_state\":\"" << SessionStateToString(newState)
           << "\"}}";
    Broadcast(stream.str());
}

void WebSocketServer::Broadcast(const std::string& message) {
    std::function<void(std::string_view)> transport;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        broadcast_messages_.push_back(message);
        transport = transport_;
    }
    if (transport) {
        transport(message);
    }
}

} // namespace recplay
