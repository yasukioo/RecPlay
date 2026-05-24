#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int expect_exists(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        return 0;
    }

    std::cerr << "Missing expected path: " << path.string() << '\n';
    return 1;
}

int expect_file_contains(const std::filesystem::path& path, const std::string& needle) {
    std::ifstream input(path);
    if (!input) {
        std::cerr << "Failed to open: " << path.string() << '\n';
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (content.find(needle) != std::string::npos) {
        return 0;
    }

    std::cerr << "Expected to find '" << needle << "' in " << path.string() << '\n';
    return 1;
}

} // namespace

int main() {
    const auto repo_root = std::filesystem::path(RECPLAY_SOURCE_DIR);

    int failures = 0;
    failures += expect_exists(repo_root / "vcpkg-overlay-ports" / "cppmicroservices" / "portfile.cmake");
    failures += expect_exists(repo_root / "vcpkg-overlay-ports" / "cppmicroservices" / "fix-msvc-14.51-spdlog-fmt.patch");
    failures += expect_file_contains(
        repo_root / "vcpkg-overlay-ports" / "cppmicroservices" / "portfile.cmake",
        "fix-msvc-14.51-spdlog-fmt.patch");
    failures += expect_file_contains(repo_root / "CMakeLists.txt", "VCPKG_OVERLAY_PORTS");
    failures += expect_file_contains(repo_root / "CMakeLists.txt", "list(FILTER _recplay_install_options EXCLUDE REGEX");

    return failures == 0 ? 0 : 1;
}
