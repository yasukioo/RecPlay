// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "BundlePathResolver.h"

#include <cppmicroservices/FrameworkFactory.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/Bundle.h>
#include <cppmicroservices/BundleContext.h>

#include <atomic>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
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
    std::filesystem::path config_path;
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
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }

        const std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && (i + 1) < argc && argv[i + 1] != nullptr) {
            return argv[i + 1];
        }

        if (!arg.empty() && arg[0] != '-') {
            return arg;
        }
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
    (void)signal_number;
    auto* framework = g_framework.load(std::memory_order_acquire);
    if (framework != nullptr) {
        framework->Stop();
    }
}

void RegisterSignalHandlers() {
    std::signal(SIGINT, HandleSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, HandleSignal);
#endif
}

void InitializeLogging() {
#if RECPLAY_HAS_SPDLOG
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    spdlog::flush_on(spdlog::level::info);
#endif
}

RuntimeConfig LoadConfig(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open config: " + path);
    }

    RuntimeConfig config;
    config.config_path = std::filesystem::absolute(path);
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

void InstallBundles(cppmicroservices::BundleContext context,
                    const RuntimeConfig& config) {
    const auto process_root = std::filesystem::current_path();
    std::vector<std::string> bundle_locations;
    std::set<std::filesystem::path> seen_locations;

    for (auto root : recplay::ResolveBundleSearchRoots(
             config.bundle_search_paths,
             config.config_path,
             process_root)) {
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
            LogInfo("bundle search path missing: " + root.string());
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
            const auto canonical_path = std::filesystem::weakly_canonical(entry.path());
            if (seen_locations.insert(canonical_path).second) {
                bundle_locations.push_back(canonical_path.string());
            }
        }
    }

    if (bundle_locations.empty()) {
        LogInfo("no bundle binaries found in configured search paths");
        return;
    }

    std::vector<cppmicroservices::Bundle> installed;
    installed.reserve(bundle_locations.size());
    for (const auto& location : bundle_locations) {
        LogInfo("installing bundle binary: " + location);
        auto bundles = context.InstallBundles(location);
        installed.insert(installed.end(), bundles.begin(), bundles.end());
    }
    LogInfo("installed " + std::to_string(installed.size()) + " bundle(s)");

    for (const auto& bundle_name : config.auto_start) {
        bool started = false;
        for (auto& bundle : installed) {
            if (bundle.GetSymbolicName() == bundle_name) {
                LogInfo("starting bundle: " + bundle_name);
                bundle.Start();
                started = true;
            }
        }
        if (!started) {
            LogInfo("auto-start bundle not found: " + bundle_name);
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        InitializeLogging();
        const auto config_path = ResolveConfigPath(argc, argv);
        RegisterSignalHandlers();
        const auto config = LoadConfig(config_path);
        LogInfo("loaded config from " + config.config_path.string());

        cppmicroservices::FrameworkFactory factory;
        auto framework = factory.NewFramework();
        g_framework.store(&framework, std::memory_order_release);
        framework.Start();
        LogInfo("framework started");

        InstallBundles(framework.GetBundleContext(), config);
        framework.WaitForStop(std::chrono::milliseconds::max());
        g_framework.store(nullptr, std::memory_order_release);
        LogInfo("framework stopped");
        return 0;
    } catch (const std::exception& ex) {
        LogError(std::string("recplay startup failed: ") + ex.what());
        return 1;
    }
}
