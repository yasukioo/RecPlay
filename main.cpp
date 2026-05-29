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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
std::atomic<bool> g_stop_requested{false};

struct RuntimeConfig {
    std::filesystem::path config_path;
    std::vector<std::string> bundle_search_paths{"bundles"};
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
    g_stop_requested.store(true, std::memory_order_release);
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

void PrependDllSearchPaths(const std::vector<std::filesystem::path>& roots) {
#ifdef _WIN32
    std::wstring current_path;
    DWORD path_size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (path_size > 0) {
        current_path.resize(path_size - 1);
        if (GetEnvironmentVariableW(L"PATH", current_path.data(), path_size) == 0) {
            current_path.clear();
        }
    }

    std::wstring updated_path;
    for (const auto& root : roots) {
        const auto root_path = root.wstring();
        if (root_path.empty()) {
            continue;
        }
        if (!updated_path.empty()) {
            updated_path.push_back(L';');
        }
        updated_path += root_path;
    }

    if (!current_path.empty()) {
        if (!updated_path.empty()) {
            updated_path.push_back(L';');
        }
        updated_path += current_path;
    }

    if (!updated_path.empty()) {
        SetEnvironmentVariableW(L"PATH", updated_path.c_str());
    }
#else
    (void)roots;
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
    const auto bundle_roots = recplay::ResolveBundleSearchRoots(
        config.bundle_search_paths,
        config.config_path,
        process_root);
    PrependDllSearchPaths(bundle_roots);
    std::vector<std::string> bundle_locations;

    for (const auto& root : bundle_roots) {
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
            LogInfo("bundle search path missing: " + root.string());
        }
    }
    for (const auto& location : recplay::DiscoverBundleBinaryLocations(
             bundle_roots, config.auto_start)) {
        bundle_locations.push_back(location.string());
    }

    if (bundle_locations.empty()) {
        LogInfo("no bundle binaries found in configured search paths");
        return;
    }

    std::vector<cppmicroservices::Bundle> installed;
    installed.reserve(bundle_locations.size());
    for (const auto& location : bundle_locations) {
        try {
            LogInfo("installing bundle binary: " + location);
            auto bundles = context.InstallBundles(location);
            installed.insert(installed.end(), bundles.begin(), bundles.end());
        } catch (const std::exception& ex) {
            LogError("failed to install bundle binary " + location + ": " + ex.what());
        } catch (...) {
            LogError("failed to install bundle binary " + location + ": unknown error");
        }
    }
    LogInfo("installed " + std::to_string(installed.size()) + " bundle(s)");

    for (const auto& bundle_name : config.auto_start) {
        bool started = false;
        for (auto& bundle : installed) {
            if (bundle.GetSymbolicName() == bundle_name) {
                try {
                    LogInfo("starting bundle: " + bundle_name);
                    bundle.Start();
                } catch (const std::exception& ex) {
                    LogError("failed to start bundle " + bundle_name + ": " + ex.what());
                } catch (...) {
                    LogError("failed to start bundle " + bundle_name + ": unknown error");
                }
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
        while (true) {
            if (g_stop_requested.exchange(false, std::memory_order_acq_rel)) {
                framework.Stop();
            }

            const auto event = framework.WaitForStop(std::chrono::milliseconds(200));
            if (event.GetType() != cppmicroservices::FrameworkEvent::Type::FRAMEWORK_WAIT_TIMEDOUT) {
                break;
            }
        }
        g_framework.store(nullptr, std::memory_order_release);
        LogInfo("framework stopped");
        return 0;
    } catch (const std::exception& ex) {
        LogError(std::string("recplay startup failed: ") + ex.what());
        return 1;
    }
}
