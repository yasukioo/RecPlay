// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "CoreEngine.h"

#include "ICodecService.h"
#include "IProtocolService.h"
#include "IStorageService.h"

#include <chrono>
#include <utility>

namespace recplay {

CoreEngine::CoreEngine() : scheduler_(timeline_) {}

TimelineEngine& CoreEngine::Timeline() { return timeline_; }
PacketScheduler& CoreEngine::Scheduler() { return scheduler_; }
InjectorBus& CoreEngine::Injector() { return injector_; }
IndexBuilder& CoreEngine::Index() { return index_; }
FilterMappingEngine& CoreEngine::Filters() { return filters_; }

void CoreEngine::SetStorageService(IStorageService* storage) {
    std::lock_guard<std::mutex> lock(mutex_);
    storage_ = storage;
}

void CoreEngine::SetCodecService(ICodecService* codec) {
    std::lock_guard<std::mutex> lock(mutex_);
    codec_ = codec;
}

IStorageService* CoreEngine::Storage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return storage_;
}

ICodecService* CoreEngine::Codec() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return codec_;
}

void CoreEngine::AttachProtocol(const std::string& name, IProtocolService* protocol) {
    if (!protocol) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto runtime = std::make_shared<ProtocolRuntime>();
    runtime->name = name;
    runtime->service = protocol;
    runtime->queue = std::make_unique<ProtocolQueue>();
    protocols_[name] = std::move(runtime);
}

void CoreEngine::DetachProtocol(const std::string& name) {
    auto runtime = FindProtocolRuntime(name);
    if (runtime != nullptr) {
        StopProtocolCapture(name);
        StopProtocolReplay(name);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    protocols_.erase(name);
}

bool CoreEngine::StartProtocolCapture(const std::string& name, const std::string& configJson) {
    auto runtime = FindProtocolRuntime(name);
    if (runtime == nullptr || runtime->service == nullptr) {
        return false;
    }
    if (runtime->capture_active.load(std::memory_order_acquire)) {
        return true;
    }

    runtime->worker_running.store(true, std::memory_order_release);
    if (!runtime->worker.joinable()) {
        runtime->worker = std::thread(&CoreEngine::RunProtocolWorker, this, runtime);
    }

    const bool started = runtime->service->StartCapture(
        configJson,
        [runtime](PacketPtr pkt) {
            if (runtime == nullptr || !runtime->worker_running.load(std::memory_order_acquire)) {
                return;
            }
            if (!pkt || runtime->queue == nullptr) {
                return;
            }

            while (runtime->worker_running.load(std::memory_order_acquire) &&
                   !runtime->queue->TryPush(pkt)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            runtime->wait_cv.notify_one();
        });

    if (!started) {
        runtime->worker_running.store(false, std::memory_order_release);
        runtime->wait_cv.notify_all();
        if (runtime->worker.joinable()) {
            runtime->worker.join();
        }
        return false;
    }

    runtime->capture_active.store(true, std::memory_order_release);
    return true;
}

void CoreEngine::StopProtocolCapture(const std::string& name) {
    auto runtime = FindProtocolRuntime(name);
    if (runtime == nullptr || runtime->service == nullptr) {
        return;
    }

    runtime->service->StopCapture();
    runtime->capture_active.store(false, std::memory_order_release);
    runtime->worker_running.store(false, std::memory_order_release);
    runtime->wait_cv.notify_all();
    if (runtime->worker.joinable()) {
        runtime->worker.join();
    }
}

bool CoreEngine::StartProtocolReplay(const std::string& name, const std::string& configJson) {
    auto runtime = FindProtocolRuntime(name);
    return runtime != nullptr &&
           runtime->service != nullptr &&
           runtime->service->StartReplay(configJson);
}

void CoreEngine::StopProtocolReplay(const std::string& name) {
    auto runtime = FindProtocolRuntime(name);
    if (runtime == nullptr || runtime->service == nullptr) {
        return;
    }
    runtime->service->StopReplay();
}

void CoreEngine::DispatchPacketToReplayingProtocols(PacketPtr pkt) {
    if (!pkt) {
        return;
    }

    std::vector<IProtocolService*> services;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        services.reserve(protocols_.size());
        for (const auto& [_, runtime] : protocols_) {
            if (runtime != nullptr && runtime->service != nullptr && runtime->service->IsReplaying()) {
                services.push_back(runtime->service);
            }
        }
    }

    for (auto* service : services) {
        service->SendPacket(pkt);
    }
}

std::vector<std::string> CoreEngine::GetAttachedProtocolNames() const {
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(mutex_);
    names.reserve(protocols_.size());
    for (const auto& [name, runtime] : protocols_) {
        if (runtime != nullptr && runtime->service != nullptr) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<ChannelInfo> CoreEngine::GetChannelsForProtocol(const std::string& name) const {
    auto runtime = FindProtocolRuntime(name);
    if (runtime == nullptr || runtime->service == nullptr) {
        return {};
    }
    return runtime->service->GetChannels();
}

std::shared_ptr<CoreEngine::ProtocolRuntime> CoreEngine::FindProtocolRuntime(
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = protocols_.find(name);
    if (it == protocols_.end()) {
        return nullptr;
    }
    return it->second;
}

void CoreEngine::RunProtocolWorker(const std::shared_ptr<ProtocolRuntime>& runtime) {
    if (runtime == nullptr || runtime->queue == nullptr) {
        return;
    }

    while (runtime->worker_running.load(std::memory_order_acquire)) {
        auto pkt = runtime->queue->TryPop();
        if (pkt.has_value()) {
            HandleCapturedPacket(std::move(*pkt));
            continue;
        }

        std::unique_lock<std::mutex> lock(runtime->wait_mutex);
        runtime->wait_cv.wait_for(lock, std::chrono::milliseconds(10), [&runtime] {
            return !runtime->worker_running.load(std::memory_order_acquire) ||
                   (runtime->queue != nullptr && !runtime->queue->IsEmpty());
        });
    }

    while (runtime->queue != nullptr) {
        auto pkt = runtime->queue->TryPop();
        if (!pkt.has_value()) {
            break;
        }
        HandleCapturedPacket(std::move(*pkt));
    }
}

void CoreEngine::HandleCapturedPacket(PacketPtr pkt) {
    if (!pkt) {
        return;
    }

    auto mapped = filters_.ApplyMapping(pkt);
    if (!mapped) {
        return;
    }

    auto injected = injector_.Process(mapped);
    if (!injected) {
        return;
    }

    IStorageService* storage = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        storage = storage_;
    }

    std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
    index_.AddPacket(
        injected->t_capture,
        0,
        injected->t_record,
        injected->channel_id,
        (injected->flags & kFlagKeyframe) != 0);

    if (storage != nullptr && storage->IsWriting()) {
        storage->WritePacket(injected);
    }
}

} // namespace recplay
