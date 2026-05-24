// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "CoreEngine.h"
#include "IProtocolService.h"
#include "ISessionService.h"
#include "IStatsService.h"
#include "RecPlayWebSocketController.h"
#include "RuntimeCatalog.h"
#include "WebRootLocator.h"
#include "WebSocketServer.h"

#include <cppmicroservices/Any.h>
#include <cppmicroservices/ServiceTracker.h>

#include <cppmicroservices/BundleActivator.h>

#include <filesystem>
#include <memory>

namespace recplay {

namespace {

std::string BuildRootDirectory() {
#ifdef RECPLAY_BUILD_ROOT
    return RECPLAY_BUILD_ROOT;
#else
    return {};
#endif
}

std::string SourceRootDirectory() {
#ifdef RECPLAY_SOURCE_ROOT
    return RECPLAY_SOURCE_ROOT;
#else
    return {};
#endif
}

} // namespace

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

class CoreTracker final : public cppmicroservices::ServiceTracker<CoreEngine> {
public:
    using Super = cppmicroservices::ServiceTracker<CoreEngine>;

    CoreTracker(cppmicroservices::BundleContext context, RuntimeCatalog* runtimeCatalog)
        : Super(context), runtime_catalog_(runtimeCatalog) {}

    std::shared_ptr<CoreEngine> AddingService(
        const cppmicroservices::ServiceReference<CoreEngine>& reference) override {
        auto service = Super::AddingService(reference);
        if (runtime_catalog_ != nullptr) {
            runtime_catalog_->SetCoreEngine(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<CoreEngine>& reference,
                        const std::shared_ptr<CoreEngine>& service) override {
        (void)reference;
        if (runtime_catalog_ != nullptr && service != nullptr) {
            runtime_catalog_->SetCoreEngine(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    RuntimeCatalog* runtime_catalog_ = nullptr;
};

class ProtocolTracker final : public cppmicroservices::ServiceTracker<IProtocolService> {
public:
    using Super = cppmicroservices::ServiceTracker<IProtocolService>;

    ProtocolTracker(cppmicroservices::BundleContext context, RuntimeCatalog* runtimeCatalog)
        : Super(context), runtime_catalog_(runtimeCatalog) {}

    std::shared_ptr<IProtocolService> AddingService(
        const cppmicroservices::ServiceReference<IProtocolService>& reference) override {
        auto service = Super::AddingService(reference);
        if (runtime_catalog_ != nullptr && service != nullptr) {
            const auto id = reference.GetProperty("protocol.name").Empty()
                ? service->GetName()
                : cppmicroservices::any_cast<std::string>(reference.GetProperty("protocol.name"));
            const auto type = reference.GetProperty("protocol.type").Empty()
                ? std::string("source")
                : cppmicroservices::any_cast<std::string>(reference.GetProperty("protocol.type"));
            runtime_catalog_->UpsertProtocol(
                id,
                service,
                reference.GetBundle().GetLocation(),
                type);
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<IProtocolService>& reference,
                        const std::shared_ptr<IProtocolService>& service) override {
        if (runtime_catalog_ != nullptr && service != nullptr) {
            const auto id = reference.GetProperty("protocol.name").Empty()
                ? service->GetName()
                : cppmicroservices::any_cast<std::string>(reference.GetProperty("protocol.name"));
            runtime_catalog_->RemoveProtocol(id, service.get());
        }
        Super::RemovedService(reference, service);
    }

private:
    RuntimeCatalog* runtime_catalog_ = nullptr;
};

class WebserverActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        http_server_ = std::make_unique<HttpServer>();
        runtime_catalog_ = std::make_shared<RuntimeCatalog>();
        http_server_->SetRuntimeCatalog(runtime_catalog_.get());
        websocket_server_ = std::make_shared<WebSocketServer>();
        RecPlayWebSocketController::SetServer(websocket_server_);
        websocket_server_->SetTransport([](std::string_view message) {
            RecPlayWebSocketController::Broadcast(message);
        });

        const auto webRoot = ResolveWebRoot(
            BuildRootDirectory(),
            SourceRootDirectory());
        if (!webRoot.empty()) {
            http_server_->SetStaticRoot(webRoot);
        }

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

        core_tracker_ = std::make_unique<CoreTracker>(context, runtime_catalog_.get());
        core_tracker_->Open();

        protocol_tracker_ = std::make_unique<ProtocolTracker>(context, runtime_catalog_.get());
        protocol_tracker_->Open();

        http_server_->Start("0.0.0.0", 8080);
        websocket_server_->Start();
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        if (protocol_tracker_) {
            protocol_tracker_->Close();
        }
        if (core_tracker_) {
            core_tracker_->Close();
        }
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
            http_server_->SetRuntimeCatalog(nullptr);
            http_server_->SetStatsService(nullptr);
            http_server_->SetSessionService(nullptr);
            http_server_->Stop();
        }
        if (websocket_server_) {
            websocket_server_->SetStatsService(nullptr);
            websocket_server_->SetSessionService(nullptr);
            websocket_server_->SetTransport({});
        }
        RecPlayWebSocketController::SetServer(nullptr);
        protocol_tracker_.reset();
        core_tracker_.reset();
        stats_tracker_.reset();
        session_tracker_.reset();
        runtime_catalog_.reset();
        websocket_server_.reset();
        http_server_.reset();
    }

private:
    std::unique_ptr<HttpServer> http_server_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    std::shared_ptr<RuntimeCatalog> runtime_catalog_;
    std::unique_ptr<SessionTracker> session_tracker_;
    std::unique_ptr<StatsTracker> stats_tracker_;
    std::unique_ptr<CoreTracker> core_tracker_;
    std::unique_ptr<ProtocolTracker> protocol_tracker_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::WebserverActivator)
