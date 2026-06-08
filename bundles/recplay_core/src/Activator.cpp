#include "CoreEngine.h"
#include "ICoreService.h"
#include "ICodecService.h"
#include "IInjectorService.h"
#include "IProtocolService.h"
#include "IStatsService.h"
#include "IStorageService.h"
#include "ITimelineService.h"

// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <cppmicroservices/Any.h>
#include <cppmicroservices/BundleActivator.h>
#include <cppmicroservices/ServiceTracker.h>

#include <memory>

namespace recplay {

class ProtocolTracker final : public cppmicroservices::ServiceTracker<IProtocolService> {
public:
    using Super = cppmicroservices::ServiceTracker<IProtocolService>;

    ProtocolTracker(cppmicroservices::BundleContext context, CoreEngine* engine)
        : Super(context), engine_(engine) {}

    std::shared_ptr<IProtocolService> AddingService(
        const cppmicroservices::ServiceReference<IProtocolService>& reference) override {
        auto service = Super::AddingService(reference);
        if (engine_ != nullptr && service != nullptr) {
            const auto protocol_name = reference.GetProperty("protocol.name").Empty()
                ? service->GetName()
                : cppmicroservices::any_cast<std::string>(reference.GetProperty("protocol.name"));
            engine_->AttachProtocol(
                protocol_name,
                service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<IProtocolService>& reference,
                        const std::shared_ptr<IProtocolService>& service) override {
        if (engine_ != nullptr && service != nullptr) {
            const auto protocol_name = reference.GetProperty("protocol.name").Empty()
                ? service->GetName()
                : cppmicroservices::any_cast<std::string>(reference.GetProperty("protocol.name"));
            engine_->DetachProtocol(
                protocol_name);
        }
        Super::RemovedService(reference, service);
    }

private:
    CoreEngine* engine_ = nullptr;
};

class CodecTracker final : public cppmicroservices::ServiceTracker<ICodecService> {
public:
    using Super = cppmicroservices::ServiceTracker<ICodecService>;

    CodecTracker(cppmicroservices::BundleContext context, CoreEngine* engine)
        : Super(context), engine_(engine) {}

    std::shared_ptr<ICodecService> AddingService(
        const cppmicroservices::ServiceReference<ICodecService>& reference) override {
        auto service = Super::AddingService(reference);
        if (engine_ != nullptr && service != nullptr) {
            engine_->SetCodecService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<ICodecService>& reference,
                        const std::shared_ptr<ICodecService>& service) override {
        (void)reference;
        if (engine_ != nullptr && service != nullptr) {
            engine_->SetCodecService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    CoreEngine* engine_ = nullptr;
};

class StorageTracker final : public cppmicroservices::ServiceTracker<IStorageService> {
public:
    using Super = cppmicroservices::ServiceTracker<IStorageService>;

    StorageTracker(cppmicroservices::BundleContext context, CoreEngine* engine)
        : Super(context), engine_(engine) {}

    std::shared_ptr<IStorageService> AddingService(
        const cppmicroservices::ServiceReference<IStorageService>& reference) override {
        auto service = Super::AddingService(reference);
        if (engine_ != nullptr && service != nullptr) {
            engine_->SetStorageService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<IStorageService>& reference,
                        const std::shared_ptr<IStorageService>& service) override {
        (void)reference;
        if (engine_ != nullptr && service != nullptr) {
            engine_->SetStorageService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    CoreEngine* engine_ = nullptr;
};

class StatsTracker final : public cppmicroservices::ServiceTracker<IStatsService> {
public:
    using Super = cppmicroservices::ServiceTracker<IStatsService>;

    StatsTracker(cppmicroservices::BundleContext context, CoreEngine* engine)
        : Super(context), engine_(engine) {}

    std::shared_ptr<IStatsService> AddingService(
        const cppmicroservices::ServiceReference<IStatsService>& reference) override {
        auto service = Super::AddingService(reference);
        if (engine_ != nullptr && service != nullptr) {
            engine_->SetStatsService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<IStatsService>& reference,
                        const std::shared_ptr<IStatsService>& service) override {
        (void)reference;
        if (engine_ != nullptr && service != nullptr) {
            engine_->SetStatsService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    CoreEngine* engine_ = nullptr;
};

class CoreActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        engine_ = std::make_shared<CoreEngine>();
        core_service_ = engine_;
        timeline_service_ = std::shared_ptr<ITimelineService>(engine_, &engine_->Timeline());
        injector_service_ = std::shared_ptr<IInjectorService>(engine_, &engine_->Injector());

        context.RegisterService<CoreEngine>(engine_);
        context.RegisterService<ICoreService>(core_service_);
        context.RegisterService<ITimelineService>(timeline_service_);
        context.RegisterService<IInjectorService>(injector_service_);

        protocol_tracker_ = std::make_unique<ProtocolTracker>(context, engine_.get());
        protocol_tracker_->Open();

        codec_tracker_ = std::make_unique<CodecTracker>(context, engine_.get());
        codec_tracker_->Open();

        storage_tracker_ = std::make_unique<StorageTracker>(context, engine_.get());
        storage_tracker_->Open();

        stats_tracker_ = std::make_unique<StatsTracker>(context, engine_.get());
        stats_tracker_->Open();
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        if (stats_tracker_) {
            stats_tracker_->Close();
        }
        if (protocol_tracker_) {
            protocol_tracker_->Close();
        }
        if (codec_tracker_) {
            codec_tracker_->Close();
        }
        if (storage_tracker_) {
            storage_tracker_->Close();
        }
        if (engine_ != nullptr) {
            engine_->SetStatsService(nullptr);
            engine_->SetCodecService(nullptr);
            engine_->SetStorageService(nullptr);
        }
        stats_tracker_.reset();
        storage_tracker_.reset();
        codec_tracker_.reset();
        protocol_tracker_.reset();
        injector_service_.reset();
        timeline_service_.reset();
        core_service_.reset();
        engine_.reset();
    }

private:
    std::shared_ptr<CoreEngine> engine_;
    std::shared_ptr<ICoreService> core_service_;
    std::shared_ptr<ITimelineService> timeline_service_;
    std::shared_ptr<IInjectorService> injector_service_;
    std::unique_ptr<ProtocolTracker> protocol_tracker_;
    std::unique_ptr<CodecTracker> codec_tracker_;
    std::unique_ptr<StorageTracker> storage_tracker_;
    std::unique_ptr<StatsTracker> stats_tracker_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::CoreActivator)
