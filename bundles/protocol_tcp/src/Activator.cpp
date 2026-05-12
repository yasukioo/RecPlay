// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IProtocolService.h"
#include "TcpProtocolService.h"

#include <cppmicroservices/BundleActivator.h>

#include <memory>
#include <string>

namespace recplay {

class TcpActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        service_ = std::make_shared<TcpProtocolService>();
        context.RegisterService<IProtocolService>(
            service_,
            {
                {"protocol.name", std::string("TCP")},
                {"protocol.priority", service_->GetPriority()},
                {"protocol.type", std::string("source")},
            });
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        service_.reset();
    }

private:
    std::shared_ptr<TcpProtocolService> service_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::TcpActivator)
