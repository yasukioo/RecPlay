// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace recplay {

struct PluginConfigFieldInfo {
    std::string key;
    std::string label;
    std::string value;
};

struct PluginRuntimeInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string state;
    std::string kind;
    std::string protocol;
    std::string desc;
    std::string bundle_path;
    std::vector<PluginConfigFieldInfo> config_fields;
};

struct RuntimeChannelInfo {
    std::string id;
    std::string name;
    std::string topic;
    std::string direction;
    std::string protocol;
    std::string plugin_id;
    uint32_t numeric_id = 0;
};

struct RuntimeTopicMapping {
    std::string source_topic;
    std::string target_topic;
};

class IRuntimeCatalog {
public:
    virtual ~IRuntimeCatalog() = default;

    virtual std::vector<PluginRuntimeInfo> ListPlugins() const = 0;
    virtual std::optional<PluginRuntimeInfo> GetPlugin(const std::string& id) const = 0;
    virtual bool SavePluginConfig(const std::string& id,
                                  const std::vector<PluginConfigFieldInfo>& fields) = 0;
    virtual bool StartPlugin(const std::string& id) = 0;
    virtual bool StopPlugin(const std::string& id) = 0;
    virtual std::vector<RuntimeChannelInfo> ListChannels() const = 0;
    virtual std::vector<RuntimeTopicMapping> GetTopicMappings() const = 0;
    virtual bool SetTopicMappings(const std::vector<RuntimeTopicMapping>& mappings) = 0;
};

} // namespace recplay
