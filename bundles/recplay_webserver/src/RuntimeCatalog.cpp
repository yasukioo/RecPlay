// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RuntimeCatalog.h"

#include "ICoreService.h"
#include "IProtocolService.h"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_RUNTIME_CATALOG_HAS_JSON 1
#else
#define RECPLAY_RUNTIME_CATALOG_HAS_JSON 0
#endif

#include <cctype>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace recplay {

namespace {

std::string HumanizeKey(const std::string& key) {
    std::string label;
    label.reserve(key.size());

    bool capitalize = true;
    for (char ch : key) {
        if (ch == '_' || ch == '-') {
            label.push_back(' ');
            capitalize = true;
            continue;
        }

        if (capitalize) {
            label.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            capitalize = false;
        } else {
            label.push_back(ch);
        }
    }

    return label;
}

std::string BuildPluginDescription(const std::string& protocol, const std::string& type) {
    const std::string kind = type.empty() ? "Service" : type;
    if (protocol.empty()) {
        return kind;
    }
    return protocol + " " + kind;
}

#if RECPLAY_RUNTIME_CATALOG_HAS_JSON
std::string JsonValueToString(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<long long>());
    }
    if (value.is_number_unsigned()) {
        return std::to_string(value.get<unsigned long long>());
    }
    if (value.is_number_float()) {
        std::ostringstream stream;
        stream << value.get<double>();
        return stream.str();
    }
    return value.dump();
}

bool TryParseTypedValue(const std::string& type,
                        const std::string& text,
                        nlohmann::json* output) {
    if (output == nullptr) {
        return false;
    }

    try {
        if (type == "integer") {
            *output = std::stoll(text);
            return true;
        }
        if (type == "number") {
            *output = std::stod(text);
            return true;
        }
        if (type == "boolean") {
            if (text == "true") {
                *output = true;
                return true;
            }
            if (text == "false") {
                *output = false;
                return true;
            }
            return false;
        }

        *output = text;
        return true;
    } catch (...) {
        return false;
    }
}
#endif

std::string ResolveChannelDirection(const std::string& type) {
    return type == "source" ? "input" : "output";
}

} // namespace

void RuntimeCatalog::SetCoreService(ICoreService* core) {
    std::lock_guard<std::mutex> lock(mutex_);
    core_ = core;
    if (core_ != nullptr) {
        ApplyMappingsLocked();
    }
}

void RuntimeCatalog::UpsertProtocol(const std::string& id,
                                    std::shared_ptr<IProtocolService> service,
                                    std::string bundlePath,
                                    std::string type) {
    if (!service) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto& entry = protocols_[id];
    entry.service = std::move(service);
    entry.bundle_path = std::move(bundlePath);
    entry.type = std::move(type);
    entry.last_error.clear();

#if RECPLAY_RUNTIME_CATALOG_HAS_JSON
    entry.schema.clear();
    entry.field_order.clear();

    try {
        const auto schema = nlohmann::json::parse(entry.service->GetConfigSchema());
        const auto properties = schema.value("properties", nlohmann::json::object());
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            SchemaField field;
            field.label = it.value().value("title", HumanizeKey(it.key()));
            field.type = it.value().value("type", std::string("string"));
            field.value = it.value().contains("default")
                ? JsonValueToString(it.value().at("default"))
                : std::string{};

            entry.schema[it.key()] = field;
            entry.field_order.push_back(it.key());
        }
    } catch (...) {
        entry.schema.clear();
        entry.field_order.clear();
    }
#else
    entry.schema.clear();
    entry.field_order.clear();
#endif
}

void RuntimeCatalog::RemoveProtocol(const std::string& id, IProtocolService* service) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = protocols_.find(id);
    if (it == protocols_.end()) {
        return;
    }
    if (service != nullptr && it->second.service.get() != service) {
        return;
    }
    protocols_.erase(it);
}

std::vector<PluginRuntimeInfo> RuntimeCatalog::ListPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PluginRuntimeInfo> plugins;
    plugins.reserve(protocols_.size());

    for (const auto& [id, _] : protocols_) {
        const auto plugin = BuildPluginLocked(id);
        if (plugin.has_value()) {
            plugins.push_back(*plugin);
        }
    }

    return plugins;
}

std::optional<PluginRuntimeInfo> RuntimeCatalog::GetPlugin(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return BuildPluginLocked(id);
}

bool RuntimeCatalog::SavePluginConfig(const std::string& id,
                                      const std::vector<PluginConfigFieldInfo>& fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = protocols_.find(id);
    if (it == protocols_.end()) {
        return false;
    }

    for (const auto& field : fields) {
        auto schemaIt = it->second.schema.find(field.key);
        if (schemaIt == it->second.schema.end()) {
            continue;
        }
        schemaIt->second.label = field.label.empty() ? schemaIt->second.label : field.label;
        schemaIt->second.value = field.value;
    }

    return true;
}

bool RuntimeCatalog::StartPlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = protocols_.find(id);
    if (it == protocols_.end() || it->second.service == nullptr || core_ == nullptr) {
        return false;
    }

#if !RECPLAY_RUNTIME_CATALOG_HAS_JSON
    it->second.last_error = "JSON support unavailable";
    return false;
#else
    nlohmann::json config = nlohmann::json::object();
    for (const auto& key : it->second.field_order) {
        const auto schemaIt = it->second.schema.find(key);
        if (schemaIt == it->second.schema.end()) {
            continue;
        }

        nlohmann::json typedValue;
        if (!TryParseTypedValue(schemaIt->second.type, schemaIt->second.value, &typedValue)) {
            it->second.last_error = "Invalid config value for " + key;
            return false;
        }
        config[key] = std::move(typedValue);
    }

    if (!core_->StartProtocolCapture(id, config.dump())) {
        it->second.last_error = "Failed to start plugin";
        return false;
    }

    it->second.last_error.clear();
    return true;
#endif
}

bool RuntimeCatalog::StopPlugin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = protocols_.find(id);
    if (it == protocols_.end() || it->second.service == nullptr || core_ == nullptr) {
        return false;
    }

    core_->StopProtocolCapture(id);
    it->second.last_error.clear();
    return true;
}

std::vector<RuntimeChannelInfo> RuntimeCatalog::ListChannels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RuntimeChannelInfo> channels;

    for (const auto& [id, entry] : protocols_) {
        if (entry.service == nullptr) {
            continue;
        }

        for (const auto& channel : entry.service->GetChannels()) {
            channels.push_back(RuntimeChannelInfo{
                id + ":" + std::to_string(channel.id),
                channel.name,
                channel.topic,
                ResolveChannelDirection(entry.type),
                channel.protocol,
                id,
                channel.id,
            });
        }
    }

    return channels;
}

std::vector<RuntimeTopicMapping> RuntimeCatalog::GetTopicMappings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mappings_;
}

bool RuntimeCatalog::SetTopicMappings(const std::vector<RuntimeTopicMapping>& mappings) {
    std::lock_guard<std::mutex> lock(mutex_);
    mappings_ = mappings;
    ApplyMappingsLocked();
    return true;
}

std::optional<PluginRuntimeInfo> RuntimeCatalog::BuildPluginLocked(const std::string& id) const {
    const auto it = protocols_.find(id);
    if (it == protocols_.end() || it->second.service == nullptr) {
        return std::nullopt;
    }

    PluginRuntimeInfo plugin;
    plugin.id = id;
    plugin.name = it->second.service->GetName();
    plugin.version = it->second.service->GetVersion();
    plugin.kind = it->second.type;
    plugin.protocol = id;
    plugin.desc = BuildPluginDescription(id, it->second.type);
    plugin.state = it->second.service->IsCapturing() || it->second.service->IsReplaying()
        ? "active"
        : (it->second.last_error.empty() ? "inactive" : "error");
    plugin.bundle_path = it->second.bundle_path;

    for (const auto& key : it->second.field_order) {
        const auto fieldIt = it->second.schema.find(key);
        if (fieldIt == it->second.schema.end()) {
            continue;
        }
        plugin.config_fields.push_back(PluginConfigFieldInfo{
            key,
            fieldIt->second.label,
            fieldIt->second.value,
        });
    }

    return plugin;
}

void RuntimeCatalog::ApplyMappingsLocked() const {
    if (core_ == nullptr) {
        return;
    }

    std::unordered_map<std::string, uint32_t> topicToChannel;
    for (const auto& [_, entry] : protocols_) {
        if (entry.service == nullptr) {
            continue;
        }
        for (const auto& channel : entry.service->GetChannels()) {
            topicToChannel.emplace(channel.topic, channel.id);
        }
    }

    std::vector<CoreTopicMapping> compiledMappings;
    for (const auto& mapping : mappings_) {
        const auto sourceIt = topicToChannel.find(mapping.source_topic);
        const auto targetIt = topicToChannel.find(mapping.target_topic);
        if (sourceIt == topicToChannel.end() || targetIt == topicToChannel.end()) {
            continue;
        }

        compiledMappings.push_back(CoreTopicMapping{
            sourceIt->second,
            targetIt->second,
            mapping.target_topic,
        });
    }

    core_->SetTopicMappings(std::move(compiledMappings));
}

} // namespace recplay
