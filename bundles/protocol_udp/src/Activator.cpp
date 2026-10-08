// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IProtocolService.h"
#include "UdpProtocolService.h"

#include <cppmicroservices/BundleActivator.h>

#include <memory>
#include <string>

namespace recplay {

class UdpActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        service_ = std::make_shared<UdpProtocolService>();
        context.RegisterService<IProtocolService>(
            service_,
            {
                {"protocol.name", std::string("UDP")},
                {"protocol.priority", service_->GetPriority()},
                {"protocol.type", std::string("source")},
            });
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        service_.reset();
    }

private:
    std::shared_ptr<UdpProtocolService> service_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::UdpActivator)
