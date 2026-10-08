// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "ICodecService.h"
#include "ZstdCodecService.h"

#include <cppmicroservices/BundleActivator.h>

#include <memory>
#include <string>

namespace recplay {

class ZstdActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        service_ = std::make_shared<ZstdCodecService>();
        context.RegisterService<ICodecService>(
            service_,
            {
                {"codec.name", std::string("zstd")},
            });
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        service_.reset();
    }

private:
    std::shared_ptr<ZstdCodecService> service_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::ZstdActivator)
