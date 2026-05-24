// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <string>

namespace recplay {

struct HttpResponse;

class StaticFileHandler {
public:
    bool SetRoot(const std::string& root);
    std::string Root() const;
    HttpResponse Handle(const std::string& path) const;

private:
    static std::string NormalizePath(const std::string& path);
    static std::string DetectContentType(const std::string& path);

    std::string root_;
};

} // namespace recplay
