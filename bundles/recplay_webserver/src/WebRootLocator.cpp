// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "WebRootLocator.h"

#include <filesystem>

namespace recplay {

namespace {

std::string ResolveIfValid(const std::filesystem::path& candidate) {
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(candidate, ec);
    if (ec || canonical.empty() || !std::filesystem::exists(canonical) ||
        !std::filesystem::is_directory(canonical) ||
        !std::filesystem::exists(canonical / "index.html")) {
        return {};
    }
    return canonical.string();
}

} // namespace

std::string ResolveWebRoot(const std::string& executableDirectory,
                           const std::string& sourceRoot) {
    const auto exeDir = std::filesystem::path(executableDirectory);
    const std::filesystem::path buildCandidates[] = {
        exeDir / "web",
        exeDir.parent_path() / "web",
    };

    for (const auto& candidate : buildCandidates) {
        if (const auto resolved = ResolveIfValid(candidate); !resolved.empty()) {
            return resolved;
        }
    }

    const auto sourceCandidate = std::filesystem::path(sourceRoot) / "web" / "dist";
    if (const auto resolved = ResolveIfValid(sourceCandidate); !resolved.empty()) {
        return resolved;
    }

    return {};
}

} // namespace recplay
