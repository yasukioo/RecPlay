// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IStatsService.h"
#include "StatsCollector.h"

#include <cppmicroservices/BundleActivator.h>

#include <memory>

namespace recplay {

class StatsActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        collector_ = std::make_shared<StatsCollector>();
        context.RegisterService<IStatsService>(collector_);
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        collector_.reset();
    }

private:
    std::shared_ptr<StatsCollector> collector_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::StatsActivator)
