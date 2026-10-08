// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "ICoreService.h"
#include "IProtocolService.h"
#include "ISessionService.h"
#include "IStatsService.h"
#include "IStorageService.h"
#include "RecPlayWebSocketController.h"
#include "RuntimeCatalog.h"
#include "WebRootLocator.h"
#include "WebSocketServer.h"

#include <cppmicroservices/Any.h>
#include <cppmicroservices/ServiceTracker.h>

#include <cppmicroservices/BundleActivator.h>

#include <filesystem>
#include <fstream>
#include <memory>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_WEBSERVER_ACTIVATOR_HAS_JSON 1
#else
#define RECPLAY_WEBSERVER_ACTIVATOR_HAS_JSON 0
#endif

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

std::string ResolveRecordingsDirectory() {
    namespace fs = std::filesystem;

    const fs::path configPath = fs::path(SourceRootDirectory()) / "config" / "recplay.json";
#if RECPLAY_WEBSERVER_ACTIVATOR_HAS_JSON
    std::error_code ec;
    if (!fs::exists(configPath, ec) || ec) {
        return "data";
    }

    std::ifstream input(configPath, std::ios::binary);
    if (!input) {
        return "data";
    }

    try {
        const auto document = nlohmann::json::parse(input, nullptr, true, true);
        std::string outputDirectory = document
            .value("storage", nlohmann::json::object())
            .value("output_directory", std::string{"data"});
        if (outputDirectory.empty()) {
            outputDirectory = "data";
        }
        const fs::path candidate(outputDirectory);
        return candidate.is_absolute()
            ? candidate.lexically_normal().string()
            : (configPath.parent_path().parent_path() / candidate).lexically_normal().string();
    } catch (...) {
        return "data";
    }
#else
    return "data";
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

class StorageTracker final : public cppmicroservices::ServiceTracker<IStorageService> {
public:
    using Super = cppmicroservices::ServiceTracker<IStorageService>;

    StorageTracker(cppmicroservices::BundleContext context, HttpServer* httpServer)
        : Super(context), http_server_(httpServer) {}

    std::shared_ptr<IStorageService> AddingService(
        const cppmicroservices::ServiceReference<IStorageService>& reference) override {
        auto service = Super::AddingService(reference);
        if (http_server_ != nullptr) {
            http_server_->SetStorageService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<IStorageService>& reference,
                        const std::shared_ptr<IStorageService>& service) override {
        (void)reference;
        if (http_server_ != nullptr && service != nullptr) {
            http_server_->SetStorageService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    HttpServer* http_server_ = nullptr;
};

class CoreTracker final : public cppmicroservices::ServiceTracker<ICoreService> {
public:
    using Super = cppmicroservices::ServiceTracker<ICoreService>;

    CoreTracker(cppmicroservices::BundleContext context, RuntimeCatalog* runtimeCatalog)
        : Super(context), runtime_catalog_(runtimeCatalog) {}

    std::shared_ptr<ICoreService> AddingService(
        const cppmicroservices::ServiceReference<ICoreService>& reference) override {
        auto service = Super::AddingService(reference);
        if (runtime_catalog_ != nullptr) {
            runtime_catalog_->SetCoreService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<ICoreService>& reference,
                        const std::shared_ptr<ICoreService>& service) override {
        (void)reference;
        if (runtime_catalog_ != nullptr && service != nullptr) {
            runtime_catalog_->SetCoreService(nullptr);
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
        http_server_->SetWebSocketServer(websocket_server_.get());

        const auto webRoot = ResolveWebRoot(
            BuildRootDirectory(),
            SourceRootDirectory());
        if (!webRoot.empty()) {
            http_server_->SetStaticRoot(webRoot);
        }
        http_server_->SetRecordingsDirectory(ResolveRecordingsDirectory());

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

        storage_tracker_ = std::make_unique<StorageTracker>(context, http_server_.get());
        storage_tracker_->Open();

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
        if (storage_tracker_) {
            storage_tracker_->Close();
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
            http_server_->SetStorageService(nullptr);
            http_server_->SetStatsService(nullptr);
            http_server_->SetSessionService(nullptr);
            http_server_->SetWebSocketServer(nullptr);
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
        storage_tracker_.reset();
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
    std::unique_ptr<StorageTracker> storage_tracker_;
    std::unique_ptr<CoreTracker> core_tracker_;
    std::unique_ptr<ProtocolTracker> protocol_tracker_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::WebserverActivator)
