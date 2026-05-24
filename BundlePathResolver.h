// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace recplay {

std::vector<std::filesystem::path> ResolveBundleSearchRoots(
    const std::vector<std::string>& rawPaths,
    const std::filesystem::path& configPath,
    const std::filesystem::path& processRoot);

std::vector<std::filesystem::path> DiscoverBundleBinaryLocations(
    const std::vector<std::filesystem::path>& searchRoots,
    const std::vector<std::string>& allowedBundleNames);

} // namespace recplay
