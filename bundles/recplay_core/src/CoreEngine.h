// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "FilterMappingEngine.h"
#include "InjectorBus.h"
#include "IndexBuilder.h"
#include "ObjectPool.h"
#include "PacketScheduler.h"
#include "TimelineEngine.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace recplay {

class IProtocolService;
class IStorageService;
class ICodecService;

class CoreEngine {
public:
    CoreEngine();

    TimelineEngine& Timeline();
    PacketScheduler& Scheduler();
    InjectorBus& Injector();
    IndexBuilder& Index();
    FilterMappingEngine& Filters();

    void AttachProtocol(const std::string& name, IProtocolService* protocol);
    void DetachProtocol(const std::string& name);

private:
    TimelineEngine timeline_;
    PacketScheduler scheduler_;
    InjectorBus injector_;
    IndexBuilder index_;
    FilterMappingEngine filters_;
    ObjectPool<Packet> packet_pool_;
    std::mutex mutex_;
    std::map<std::string, IProtocolService*> protocols_;
};

} // namespace recplay
