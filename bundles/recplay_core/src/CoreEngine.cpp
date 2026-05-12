// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "CoreEngine.h"

namespace recplay {

CoreEngine::CoreEngine() : scheduler_(timeline_) {}

TimelineEngine& CoreEngine::Timeline() { return timeline_; }
PacketScheduler& CoreEngine::Scheduler() { return scheduler_; }
InjectorBus& CoreEngine::Injector() { return injector_; }
IndexBuilder& CoreEngine::Index() { return index_; }
FilterMappingEngine& CoreEngine::Filters() { return filters_; }

void CoreEngine::AttachProtocol(const std::string& name, IProtocolService* protocol) {
    if (!protocol) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    protocols_[name] = protocol;
}

void CoreEngine::DetachProtocol(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    protocols_.erase(name);
}

} // namespace recplay
