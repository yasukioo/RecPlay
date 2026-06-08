// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "FilterMappingEngine.h"
#include "ICoreService.h"
#include "InjectorBus.h"
#include "IndexBuilder.h"
#include "ObjectPool.h"
#include "PacketScheduler.h"
#include "RingBuffer.h"
#include "TimelineEngine.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace recplay {

class IProtocolService;
class IStorageService;
class ICodecService;
class IStatsService;

class CoreEngine final : public ICoreService {
public:
    CoreEngine();

    TimelineEngine& Timeline();
    PacketScheduler& Scheduler();
    InjectorBus& Injector();
    IndexBuilder& Index();
    FilterMappingEngine& Filters();

    void SetStorageService(IStorageService* storage);
    void SetCodecService(ICodecService* codec);
    void SetStatsService(IStatsService* stats);
    IStorageService* Storage() const;
    ICodecService* Codec() const;

    void AttachProtocol(const std::string& name, IProtocolService* protocol);
    void DetachProtocol(const std::string& name);
    bool StartProtocolCapture(const std::string& name, const std::string& configJson) override;
    void StopProtocolCapture(const std::string& name) override;
    bool StartProtocolReplay(const std::string& name, const std::string& configJson);
    void StopProtocolReplay(const std::string& name);
    void DispatchPacketToReplayingProtocols(PacketPtr pkt);
    std::vector<std::string> GetAttachedProtocolNames() const;
    std::vector<ChannelInfo> GetChannelsForProtocol(const std::string& name) const;
    void SetTopicMappings(const std::vector<CoreTopicMapping>& mappings) override;

private:
    using ProtocolQueue = SPSCRingBuffer<PacketPtr, 1024>;

    struct ProtocolRuntime {
        std::string name;
        IProtocolService* service = nullptr;
        std::unique_ptr<ProtocolQueue> queue;
        std::mutex wait_mutex;
        std::condition_variable wait_cv;
        std::atomic<bool> worker_running{false};
        std::atomic<bool> capture_active{false};
        std::thread worker;
    };

    std::shared_ptr<ProtocolRuntime> FindProtocolRuntime(const std::string& name) const;
    void RunProtocolWorker(const std::shared_ptr<ProtocolRuntime>& runtime);
    void HandleCapturedPacket(PacketPtr pkt);

    TimelineEngine timeline_;
    PacketScheduler scheduler_;
    InjectorBus injector_;
    IndexBuilder index_;
    FilterMappingEngine filters_;
    ObjectPool<Packet> packet_pool_;
    mutable std::mutex mutex_;
    std::mutex pipeline_mutex_;
    std::map<std::string, std::shared_ptr<ProtocolRuntime>> protocols_;
    IStorageService* storage_ = nullptr;
    ICodecService* codec_ = nullptr;
    IStatsService* stats_ = nullptr;
};

} // namespace recplay
