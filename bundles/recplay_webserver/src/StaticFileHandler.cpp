// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StaticFileHandler.h"

#include "HttpServer.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace recplay {

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
        const auto rootPathString = rootPath.string();
        const auto requestedPathString = requestedPath.string();
        const bool insideRoot =
            requestedPathString.size() >= rootPathString.size() &&
            requestedPathString.compare(0, rootPathString.size(), rootPathString) == 0;

        if (insideRoot && std::filesystem::exists(requestedPath) &&
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
