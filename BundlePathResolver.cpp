// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "BundlePathResolver.h"

#include <array>
#include <set>

namespace recplay {

namespace {

bool IsBuildConfigDirectoryName(const std::filesystem::path& path) {
    static constexpr std::array<const char*, 4> kConfigNames{
        "Debug",
        "Release",
        "RelWithDebInfo",
        "MinSizeRel",
    };

    const auto name = path.filename().string();
    for (const auto* candidate : kConfigNames) {
        if (name == candidate) {
            return true;
        }
    }
    return false;
}

} // namespace

std::filesystem::path ResolveBundleBaseRoot(
    const std::filesystem::path& configPath,
    const std::filesystem::path& processRoot) {
    const bool hasConfigDirectory = IsBuildConfigDirectoryName(processRoot);
    if (hasConfigDirectory) {
        return processRoot.parent_path();
    }

    if (!configPath.empty()) {
        const auto configDirectory = configPath.parent_path();
        if (!configDirectory.empty()) {
            return configDirectory;
        }
    }

    return processRoot;
}

std::vector<std::filesystem::path> ResolveBundleSearchRoots(
    const std::vector<std::string>& rawPaths,
    const std::filesystem::path& configPath,
    const std::filesystem::path& processRoot) {
    std::vector<std::filesystem::path> resolved;
    resolved.reserve(rawPaths.size());
    const bool hasConfigDirectory = IsBuildConfigDirectoryName(processRoot);
    const auto baseRoot = ResolveBundleBaseRoot(configPath, processRoot);

    for (const auto& rawPath : rawPaths) {
        auto root = std::filesystem::path(rawPath);
        if (root.is_relative()) {
            if (root == std::filesystem::path("bundles") && hasConfigDirectory) {
                const auto localBundles = processRoot / "bundles";
                if (std::filesystem::exists(localBundles) && std::filesystem::is_directory(localBundles)) {
                    root = localBundles;
                } else {
                    const auto configScoped = baseRoot / "bundles" / processRoot.filename();
                    if (std::filesystem::exists(configScoped) && std::filesystem::is_directory(configScoped)) {
                        root = configScoped;
                    } else {
                        root = baseRoot / root;
                    }
                }
            } else {
                root = baseRoot / root;
            }
        }
        resolved.push_back(std::filesystem::weakly_canonical(root));
    }

    return resolved;
}

std::vector<std::filesystem::path> DiscoverBundleBinaryLocations(
    const std::vector<std::filesystem::path>& searchRoots,
    const std::vector<std::string>& allowedBundleNames) {
    std::set<std::string> allowedNames(allowedBundleNames.begin(), allowedBundleNames.end());
    std::set<std::filesystem::path> seenLocations;
    std::vector<std::filesystem::path> bundleLocations;

    for (const auto& root : searchRoots) {
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

            if (allowedNames.find(entry.path().stem().string()) == allowedNames.end()) {
                continue;
            }

            const auto canonicalPath = std::filesystem::weakly_canonical(entry.path());
            if (seenLocations.insert(canonicalPath).second) {
                bundleLocations.push_back(canonicalPath);
            }
        }
    }

    return bundleLocations;
}

} // namespace recplay
