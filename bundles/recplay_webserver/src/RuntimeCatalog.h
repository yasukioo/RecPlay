// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IRuntimeCatalog.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace recplay {

class ICoreService;
class IProtocolService;

class RuntimeCatalog final : public IRuntimeCatalog {
public:
    RuntimeCatalog() = default;

    void SetCoreService(ICoreService* core);
    void UpsertProtocol(const std::string& id,
                        std::shared_ptr<IProtocolService> service,
                        std::string bundlePath,
                        std::string type);
    void RemoveProtocol(const std::string& id, IProtocolService* service);

    std::vector<PluginRuntimeInfo> ListPlugins() const override;
    std::optional<PluginRuntimeInfo> GetPlugin(const std::string& id) const override;
    bool SavePluginConfig(const std::string& id,
                          const std::vector<PluginConfigFieldInfo>& fields) override;
    bool StartPlugin(const std::string& id) override;
    bool StopPlugin(const std::string& id) override;
    std::vector<RuntimeChannelInfo> ListChannels() const override;
    std::vector<RuntimeTopicMapping> GetTopicMappings() const override;
    bool SetTopicMappings(const std::vector<RuntimeTopicMapping>& mappings) override;

private:
    struct SchemaField {
        std::string label;
        std::string type;
        std::string value;
    };

    struct ProtocolEntry {
        std::shared_ptr<IProtocolService> service;
        std::string bundle_path;
        std::string type;
        std::map<std::string, SchemaField> schema;
        std::vector<std::string> field_order;
        std::string last_error;
    };

    std::optional<PluginRuntimeInfo> BuildPluginLocked(const std::string& id) const;
    void ApplyMappingsLocked() const;

    mutable std::mutex mutex_;
    ICoreService* core_ = nullptr;
    std::map<std::string, ProtocolEntry> protocols_;
    std::vector<RuntimeTopicMapping> mappings_;
};

} // namespace recplay
