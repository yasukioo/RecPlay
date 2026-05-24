// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
    std::filesystem::create_directories(tempRoot / "runtime" / "bundles" / "Debug");
    std::filesystem::create_directories(tempRoot / "config");

    const auto resolved = recplay::ResolveBundleSearchRoots(
        {"bundles", "build/bundles"},
        tempRoot / "config" / "recplay.json",
        tempRoot / "runtime" / "Debug");

    Expect(resolved.size() == 2, "resolver should keep both configured search roots");
    Expect(resolved[0] == std::filesystem::weakly_canonical(tempRoot / "runtime" / "bundles" / "Debug"),
           "relative bundles path should resolve to config-specific bundle output");
    const auto expectedBuildRoot =
        std::filesystem::weakly_canonical(tempRoot / "runtime" / "build" / "bundles");
    Expect(
        resolved[1] == expectedBuildRoot,
        "relative build path should still resolve against process root when requested; expected=" +
            expectedBuildRoot.string() + ", actual=" + resolved[1].string());

    std::filesystem::remove_all(tempRoot);
}

void TestDiscoverBundleBinaryLocationsFiltersThirdPartyLibraries() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_bundle_discovery_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "bundles");

    std::ofstream(tempRoot / "bundles" / "recplay_webserver.dll").put('\n');
    std::ofstream(tempRoot / "bundles" / "protocol_udp.dll").put('\n');
    std::ofstream(tempRoot / "bundles" / "brotlicommon.dll").put('\n');

    const auto discovered = recplay::DiscoverBundleBinaryLocations(
        {tempRoot / "bundles"},
        {"recplay_webserver", "protocol_udp"});

    Expect(discovered.size() == 2, "discovery should keep only configured bundle binaries");
    Expect(discovered[0].filename().string() == "protocol_udp.dll" ||
               discovered[0].filename().string() == "recplay_webserver.dll",
           "discovery should return bundle dll paths");
    Expect(discovered[1].filename().string() == "protocol_udp.dll" ||
               discovered[1].filename().string() == "recplay_webserver.dll",
           "discovery should return bundle dll paths");

    std::filesystem::remove_all(tempRoot);
}

} // namespace

int main() {
    try {
        TestRelativeBundleRootsPreferProcessRoot();
        TestDiscoverBundleBinaryLocationsFiltersThirdPartyLibraries();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
