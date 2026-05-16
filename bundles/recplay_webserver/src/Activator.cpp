// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "ISessionService.h"
#include "IStatsService.h"
#include "WebSocketServer.h"

#include <cppmicroservices/ServiceTracker.h>

#include <cppmicroservices/BundleActivator.h>

#include <memory>

namespace recplay {

class SessionTracker final : public cppmicroservices::ServiceTracker<ISessionService> {
public:
    using Super = cppmicroservices::ServiceTracker<ISessionService>;

    SessionTracker(cppmicroservices::BundleContext context,
                   HttpServer* httpServer,
                   WebSocketServer* webSocketServer)
        : Super(context),
          http_server_(httpServer),
          websocket_server_(webSocketServer) {}

    std::shared_ptr<ISessionService> AddingService(
        const cppmicroservices::ServiceReference<ISessionService>& reference) override {
        auto service = Super::AddingService(reference);
        if (http_server_ != nullptr) {
            http_server_->SetSessionService(service.get());
        }
        if (websocket_server_ != nullptr) {
            websocket_server_->SetSessionService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<ISessionService>& reference,
                        const std::shared_ptr<ISessionService>& service) override {
        (void)reference;
        if (http_server_ != nullptr && service != nullptr) {
            http_server_->SetSessionService(nullptr);
        }
        if (websocket_server_ != nullptr && service != nullptr) {
            websocket_server_->SetSessionService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    HttpServer* http_server_ = nullptr;
    WebSocketServer* websocket_server_ = nullptr;
};

class StatsTracker final : public cppmicroservices::ServiceTracker<IStatsService> {
public:
    using Super = cppmicroservices::ServiceTracker<IStatsService>;

    StatsTracker(cppmicroservices::BundleContext context,
                 HttpServer* httpServer,
                 WebSocketServer* webSocketServer)
        : Super(context),
          http_server_(httpServer),
          websocket_server_(webSocketServer) {}

    std::shared_ptr<IStatsService> AddingService(
        const cppmicroservices::ServiceReference<IStatsService>& reference) override {
        auto service = Super::AddingService(reference);
        if (http_server_ != nullptr) {
            http_server_->SetStatsService(service.get());
        }
        if (websocket_server_ != nullptr) {
            websocket_server_->SetStatsService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<IStatsService>& reference,
                        const std::shared_ptr<IStatsService>& service) override {
        (void)reference;
        if (http_server_ != nullptr && service != nullptr) {
            http_server_->SetStatsService(nullptr);
        }
        if (websocket_server_ != nullptr && service != nullptr) {
            websocket_server_->SetStatsService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    HttpServer* http_server_ = nullptr;
    WebSocketServer* websocket_server_ = nullptr;
};

class WebserverActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        http_server_ = std::make_unique<HttpServer>();
        websocket_server_ = std::make_unique<WebSocketServer>();

        session_tracker_ = std::make_unique<SessionTracker>(
            context,
            http_server_.get(),
            websocket_server_.get());
        session_tracker_->Open();

        stats_tracker_ = std::make_unique<StatsTracker>(
            context,
            http_server_.get(),
            websocket_server_.get());
        stats_tracker_->Open();

        http_server_->Start("0.0.0.0", 8080);
        websocket_server_->Start();
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        if (stats_tracker_) {
            stats_tracker_->Close();
        }
        if (session_tracker_) {
            session_tracker_->Close();
        }
        if (websocket_server_) {
            websocket_server_->Stop();
        }
        if (http_server_) {
            http_server_->SetStatsService(nullptr);
            http_server_->SetSessionService(nullptr);
            http_server_->Stop();
        }
        if (websocket_server_) {
            websocket_server_->SetStatsService(nullptr);
            websocket_server_->SetSessionService(nullptr);
        }
        stats_tracker_.reset();
        session_tracker_.reset();
        websocket_server_.reset();
        http_server_.reset();
    }

private:
    std::unique_ptr<HttpServer> http_server_;
    std::unique_ptr<WebSocketServer> websocket_server_;
    std::unique_ptr<SessionTracker> session_tracker_;
    std::unique_ptr<StatsTracker> stats_tracker_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::WebserverActivator)
