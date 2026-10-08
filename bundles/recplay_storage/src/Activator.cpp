#include "ICodecService.h"
#include "IStorageService.h"
#include "StorageService.h"

// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <cppmicroservices/BundleActivator.h>
#include <cppmicroservices/ServiceTracker.h>

#include <memory>

namespace recplay {

class CodecTracker final : public cppmicroservices::ServiceTracker<ICodecService> {
public:
    using Super = cppmicroservices::ServiceTracker<ICodecService>;

    CodecTracker(cppmicroservices::BundleContext context, StorageService* service)
        : Super(context), service_(service) {}

    std::shared_ptr<ICodecService> AddingService(
        const cppmicroservices::ServiceReference<ICodecService>& reference) override {
        auto service = Super::AddingService(reference);
        if (service_ != nullptr && service != nullptr) {
            service_->SetCodecService(service.get());
        }
        return service;
    }

    void RemovedService(const cppmicroservices::ServiceReference<ICodecService>& reference,
                        const std::shared_ptr<ICodecService>& service) override {
        (void)reference;
        if (service_ != nullptr && service != nullptr) {
            service_->SetCodecService(nullptr);
        }
        Super::RemovedService(reference, service);
    }

private:
    StorageService* service_ = nullptr;
};

class StorageActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        service_ = std::make_shared<StorageService>();
        context.RegisterService<IStorageService>(service_);
        codec_tracker_ = std::make_unique<CodecTracker>(context, service_.get());
        codec_tracker_->Open();
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        if (codec_tracker_) {
            codec_tracker_->Close();
        }
        if (service_ != nullptr) {
            service_->SetCodecService(nullptr);
        }
        codec_tracker_.reset();
        service_.reset();
    }

private:
    std::shared_ptr<StorageService> service_;
    std::unique_ptr<CodecTracker> codec_tracker_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::StorageActivator)
