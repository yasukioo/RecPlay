// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StaticFileHandler.h"

#include "HttpServer.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cwctype>
#include <sstream>

namespace recplay {

namespace {

constexpr std::uintmax_t kMaxStaticFileBytes = 16ull * 1024ull * 1024ull;

bool IsPathWithinRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
#if defined(_WIN32)
    const auto rootValue = root.native();
    const auto candidateValue = candidate.native();
    if (candidateValue.size() < rootValue.size()) {
        return false;
    }
    for (size_t i = 0; i < rootValue.size(); ++i) {
        if (std::towlower(rootValue[i]) != std::towlower(candidateValue[i])) {
            return false;
        }
    }
    if (candidateValue.size() == rootValue.size()) {
        return true;
    }
    const auto separator = candidateValue[rootValue.size()];
    return separator == L'\\' || separator == L'/';
#else
    const auto rootValue = root.native();
    const auto candidateValue = candidate.native();
    if (candidateValue.size() < rootValue.size()) {
        return false;
    }
    if (candidateValue.compare(0, rootValue.size(), rootValue) != 0) {
        return false;
    }
    if (candidateValue.size() == rootValue.size()) {
        return true;
    }
    return candidateValue[rootValue.size()] == '/';
#endif
}

} // namespace

bool StaticFileHandler::SetRoot(const std::string& root) {
    if (root.empty()) {
        root_.clear();
        return true;
    }

    std::error_code ec;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, ec);
    if (ec || canonicalRoot.empty() || !std::filesystem::exists(canonicalRoot) ||
        !std::filesystem::is_directory(canonicalRoot)) {
        return false;
    }

    root_ = canonicalRoot.string();
    return true;
}

std::string StaticFileHandler::Root() const {
    return root_;
}

HttpResponse StaticFileHandler::Handle(const std::string& path) const {
    if (root_.empty()) {
        return HttpResponse{404, "application/json", "{\"error\":\"Not Found\"}"};
    }

    const std::string normalized = NormalizePath(path);
    const bool prefersSpaFallback = normalized.find('.') == std::string::npos;

    std::error_code ec;
    const auto rootPath = std::filesystem::path(root_);

    auto resolvePath = [&](const std::string& relativePath) {
        return std::filesystem::weakly_canonical(rootPath / relativePath, ec);
    };

    auto serveFile = [&](const std::filesystem::path& filePath) -> HttpResponse {
        std::error_code fileSizeError;
        const auto fileSize = std::filesystem::file_size(filePath, fileSizeError);
        if (fileSizeError) {
            return HttpResponse{404, "application/json", "{\"error\":\"Not Found\"}"};
        }
        if (fileSize > kMaxStaticFileBytes) {
            return HttpResponse{
                413,
                "application/json",
                "{\"error\":\"Static file exceeds size limit\"}"};
        }

        std::ifstream input(filePath, std::ios::binary);
        if (!input) {
            return HttpResponse{404, "application/json", "{\"error\":\"Not Found\"}"};
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        return HttpResponse{200, DetectContentType(filePath.string()), buffer.str()};
    };

    const auto requestedPath = resolvePath(normalized);
    if (!ec && !requestedPath.empty()) {
        if (IsPathWithinRoot(rootPath, requestedPath) && std::filesystem::exists(requestedPath) &&
            std::filesystem::is_regular_file(requestedPath)) {
            return serveFile(requestedPath);
        }
    }

    if (prefersSpaFallback) {
        ec.clear();
        const auto indexPath = std::filesystem::weakly_canonical(rootPath / "index.html", ec);
        if (!ec && std::filesystem::exists(indexPath) && std::filesystem::is_regular_file(indexPath)) {
            return serveFile(indexPath);
        }
    }

    return HttpResponse{404, "application/json", "{\"error\":\"Not Found\"}"};
}

std::string StaticFileHandler::NormalizePath(const std::string& path) {
    if (path.empty() || path == "/") {
        return "index.html";
    }

    std::string normalized = path;
    if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
    }

    if (normalized.empty()) {
        return "index.html";
    }

    return normalized;
}

std::string StaticFileHandler::DetectContentType(const std::string& path) {
    const auto extension = std::filesystem::path(path).extension().string();
    if (extension == ".html") {
        return "text/html";
    }
    if (extension == ".js") {
        return "application/javascript";
    }
    if (extension == ".css") {
        return "text/css";
    }
    if (extension == ".json") {
        return "application/json";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }
    return "application/octet-stream";
}

} // namespace recplay
