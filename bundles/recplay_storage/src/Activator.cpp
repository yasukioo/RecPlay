#include "IStorageService.h"
#include "StorageService.h"

// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <cppmicroservices/BundleActivator.h>

#include <memory>

namespace recplay {

class StorageActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        service_ = std::make_shared<StorageService>();
        context.RegisterService<IStorageService>(service_);
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        service_.reset();
    }

private:
    std::shared_ptr<StorageService> service_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::StorageActivator)
