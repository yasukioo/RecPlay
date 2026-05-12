// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <cppmicroservices/FrameworkFactory.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/Bundle.h>

#include <atomic>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#endif

#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#define RECPLAY_HAS_SPDLOG 1
#else
#define RECPLAY_HAS_SPDLOG 0
#endif

namespace {

std::atomic<cppmicroservices::Framework*> g_framework{nullptr};

struct RuntimeConfig {
    std::vector<std::string> bundle_search_paths{"bundles", "build/bundles"};
    std::vector<std::string> auto_start{
        "recplay_core",
        "recplay_session",
        "recplay_webserver",
        "recplay_storage",
        "recplay_stats",
        "protocol_udp",
        "protocol_tcp",
        "codec_zstd",
    };
};

std::string ResolveConfigPath(int argc, char* argv[]) {
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]).size() > 0) {
        return argv[1];
    }
    return "config/recplay.json";
}

void LogInfo(const std::string& message) {
#if RECPLAY_HAS_SPDLOG
    spdlog::info(message);
#else
    std::clog << "[INFO] " << message << '\n';
#endif
}

void LogError(const std::string& message) {
#if RECPLAY_HAS_SPDLOG
    spdlog::error(message);
#else
    std::cerr << "[ERROR] " << message << '\n';
#endif
}

void HandleSignal(int signal_number) {
    auto* framework = g_framework.load(std::memory_order_acquire);
    if (framework != nullptr) {
        LogInfo("received shutdown signal " + std::to_string(signal_number));
        framework->Stop();
    }
}

void RegisterSignalHandlers() {
    std::signal(SIGINT, HandleSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, HandleSignal);
#endif
}

RuntimeConfig LoadConfig(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open config: " + path);
    }

    RuntimeConfig config;
#if __has_include(<nlohmann/json.hpp>)
    nlohmann::json document;
    input >> document;

    if (document.contains("bundles")) {
        const auto& bundles = document.at("bundles");
        if (bundles.contains("search_paths")) {
            config.bundle_search_paths = bundles.at("search_paths").get<std::vector<std::string>>();
        }
        if (bundles.contains("auto_start")) {
            config.auto_start = bundles.at("auto_start").get<std::vector<std::string>>();
        }
    }
#else
    (void)input;
    throw std::runtime_error("nlohmann_json is required to parse config: " + path);
#endif
    return config;
}

void InstallBundles(const cppmicroservices::BundleContext& context,
                    const RuntimeConfig& config) {
    std::vector<std::string> bundle_locations;

    for (const auto& raw_path : config.bundle_search_paths) {
        const std::filesystem::path root(raw_path);
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
            continue;
        }

        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            if (ext != ".dll" && ext != ".so" && ext != ".dylib") {
                continue;
            }
            bundle_locations.push_back(entry.path().string());
        }
    }

    if (bundle_locations.empty()) {
        LogInfo("no bundle binaries found in configured search paths");
        return;
    }

    auto installed = context.InstallBundles(bundle_locations);
    LogInfo("installed " + std::to_string(installed.size()) + " bundle(s)");

    for (const auto& bundle_name : config.auto_start) {
        for (auto& bundle : installed) {
            if (bundle.GetSymbolicName() == bundle_name) {
                LogInfo("starting bundle: " + bundle_name);
                bundle.Start();
            }
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto config_path = ResolveConfigPath(argc, argv);
        RegisterSignalHandlers();
        const auto config = LoadConfig(config_path);
        LogInfo("loaded config from " + config_path);

        cppmicroservices::FrameworkFactory factory;
        auto framework = factory.NewFramework();
        g_framework.store(&framework, std::memory_order_release);
        framework.Start();
        LogInfo("framework started");

        InstallBundles(framework.GetBundleContext(), config);
        framework.WaitForStop();
        g_framework.store(nullptr, std::memory_order_release);
        LogInfo("framework stopped");
        return 0;
    } catch (const std::exception& ex) {
        LogError(std::string("recplay startup failed: ") + ex.what());
        return 1;
    }
}
