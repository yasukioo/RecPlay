// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "BundlePathResolver.h"

namespace {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestRelativeBundleRootsPreferProcessRoot() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_bundle_path_resolution_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "runtime" / "Debug" / "bundles");
    std::filesystem::create_directories(tempRoot / "config");

    const auto resolved = recplay::ResolveBundleSearchRoots(
        {"bundles"},
        tempRoot / "config" / "recplay.json",
        tempRoot / "runtime" / "Debug");

    Expect(resolved.size() == 1, "resolver should keep configured search roots");
    Expect(resolved[0] == std::filesystem::weakly_canonical(tempRoot / "runtime" / "Debug" / "bundles"),
           "relative bundles path should resolve to config-local bundle output");

    std::filesystem::remove_all(tempRoot);
}

void TestRelativeBundleRootsUseConfigDirectoryOutsideBuildLayout() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_bundle_config_root_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "config");
    std::filesystem::create_directories(tempRoot / "config" / "bundles");

    const auto resolved = recplay::ResolveBundleSearchRoots(
        {"bundles"},
        tempRoot / "config" / "recplay.json",
        tempRoot / "runtime");

    Expect(resolved.size() == 1, "resolver should keep configured search roots");
    Expect(resolved[0] == std::filesystem::weakly_canonical(tempRoot / "config" / "bundles"),
           "relative bundles path should resolve relative to config directory outside build layout");

    std::filesystem::remove_all(tempRoot);
}

} // namespace

int main() {
    try {
        TestRelativeBundleRootsPreferProcessRoot();
        TestRelativeBundleRootsUseConfigDirectoryOutsideBuildLayout();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
