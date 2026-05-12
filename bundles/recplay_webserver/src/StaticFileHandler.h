// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <string>

namespace recplay {

class StaticFileHandler {
public:
    bool SetRoot(const std::string& root);
    std::string Root() const;

private:
    std::string root_;
};

} // namespace recplay
