// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "WebSocketServer.h"

#include "DrogonWindowsCompat.h"
#include "IStatsService.h"
#include "ISessionService.h"

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define RECPLAY_HAS_DROGON 1
#else
#define RECPLAY_HAS_DROGON 0
#endif

#include <cmath>
#include <sstream>

namespace recplay {

namespace {

constexpr size_t kMaxBroadcastHistory = 256;

double SanitizeJsonNumber(double value) {
    return std::isfinite(value) ? value : 0.0;
}

void BroadcastToSharedState(
    const std::shared_ptr<WebSocketServer::SharedState>& state,
    const std::string& message) {
    std::function<void(std::string_view)> transport;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->running) {
            return;
        }
        state->broadcast_messages.push_back(message);
        if (state->broadcast_messages.size() > kMaxBroadcastHistory) {
            state->broadcast_messages.erase(state->broadcast_messages.begin());
        }
        transport = state->transport;
    }
    if (transport) {
        transport(message);
    }
}

} // namespace

WebSocketServer::WebSocketServer(ISessionService* session, IStatsService* stats)
    : shared_state_(std::make_shared<SharedState>()) {
    shared_state_->session.store(session, std::memory_order_release);
    shared_state_->stats.store(stats, std::memory_order_release);
}

bool WebSocketServer::Start() {
#if RECPLAY_HAS_DROGON
    (void)drogon::app().getLoop();
#endif
    {
        std::lock_guard<std::mutex> lock(shared_state_->mutex);
        shared_state_->running = true;
    }
    BindSubscriptions();
    return true;
}

void WebSocketServer::Stop() {
    std::lock_guard<std::mutex> lock(shared_state_->mutex);
    shared_state_->running = false;
    ++shared_state_->session_subscription_generation;
    ++shared_state_->stats_subscription_generation;
}

bool WebSocketServer::IsRunning() const {
    std::lock_guard<std::mutex> lock(shared_state_->mutex);
    return shared_state_->running;
}

void WebSocketServer::SetSessionService(ISessionService* session) {
    shared_state_->session.store(session, std::memory_order_release);
    if (IsRunning()) {
        BindSubscriptions();
    } else {
        ++shared_state_->session_subscription_generation;
    }
}

void WebSocketServer::SetStatsService(IStatsService* stats) {
    shared_state_->stats.store(stats, std::memory_order_release);
    if (IsRunning()) {
        BindSubscriptions();
    } else {
        ++shared_state_->stats_subscription_generation;
    }
}

std::vector<std::string> WebSocketServer::GetBroadcastMessages() const {
    std::lock_guard<std::mutex> lock(shared_state_->mutex);
    return shared_state_->broadcast_messages;
}

void WebSocketServer::ClearBroadcastMessages() {
    std::lock_guard<std::mutex> lock(shared_state_->mutex);
    shared_state_->broadcast_messages.clear();
}

void WebSocketServer::SetTransport(std::function<void(std::string_view)> transport) {
    std::lock_guard<std::mutex> lock(shared_state_->mutex);
    shared_state_->transport = std::move(transport);
}

void WebSocketServer::BindSubscriptions() {
    auto stats = shared_state_->stats.load(std::memory_order_acquire);
    if (stats != nullptr) {
        const uint64_t generation = ++shared_state_->stats_subscription_generation;
        auto state = shared_state_;
        stats->OnUpdate([state, generation](const StatsSnapshot& snapshot) {
            if (generation != state->stats_subscription_generation.load(std::memory_order_acquire)) {
                return;
            }
            std::ostringstream stream;
            stream << "{\"type\":\"stats\""
                   << ",\"data\":{\"total_packets\":" << snapshot.total_packets
                   << ",\"total_drops\":" << snapshot.total_drops
                   << ",\"total_throughput_mbps\":" << SanitizeJsonNumber(snapshot.total_throughput_mbps)
                   << ",\"cpu_usage_percent\":";
            if (snapshot.cpu_usage_percent.has_value() &&
                std::isfinite(*snapshot.cpu_usage_percent)) {
                stream << *snapshot.cpu_usage_percent;
            } else {
                stream << "null";
            }
            stream << "}}";
            BroadcastToSharedState(state, stream.str());
        });
    }
    auto session = shared_state_->session.load(std::memory_order_acquire);
    if (session != nullptr) {
        const uint64_t generation = ++shared_state_->session_subscription_generation;
        auto state = shared_state_;
        session->OnStateChanged([state, generation](SessionState oldState, SessionState newState) {
            if (generation != state->session_subscription_generation.load(std::memory_order_acquire)) {
                return;
            }
            std::ostringstream stream;
            stream << "{\"type\":\"state_changed\""
                   << ",\"data\":{\"old_state\":\"" << SessionStateToString(oldState)
                   << "\",\"new_state\":\"" << SessionStateToString(newState)
                   << "\"}}";
            BroadcastToSharedState(state, stream.str());
        });
    }
}

void WebSocketServer::HandleStatsUpdate(const StatsSnapshot& snapshot) {
    std::ostringstream stream;
    stream << "{\"type\":\"stats\""
           << ",\"data\":{\"total_packets\":" << snapshot.total_packets
           << ",\"total_drops\":" << snapshot.total_drops
           << ",\"total_throughput_mbps\":" << SanitizeJsonNumber(snapshot.total_throughput_mbps)
           << ",\"cpu_usage_percent\":";
    if (snapshot.cpu_usage_percent.has_value() &&
        std::isfinite(*snapshot.cpu_usage_percent)) {
        stream << *snapshot.cpu_usage_percent;
    } else {
        stream << "null";
    }
    stream << "}}";
    BroadcastToSharedState(shared_state_, stream.str());
}

void WebSocketServer::HandleStateChanged(SessionState oldState, SessionState newState) {
    std::ostringstream stream;
    stream << "{\"type\":\"state_changed\""
           << ",\"data\":{\"old_state\":\"" << SessionStateToString(oldState)
           << "\",\"new_state\":\"" << SessionStateToString(newState)
           << "\"}}";
    BroadcastToSharedState(shared_state_, stream.str());
}

void WebSocketServer::Broadcast(const std::string& message) {
    BroadcastToSharedState(shared_state_, message);
}

} // namespace recplay
