// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "StaticFileHandler.h"

namespace recplay {

bool StaticFileHandler::SetRoot(const std::string& root) {
    root_ = root;
    return true;
}

std::string StaticFileHandler::Root() const {
    return root_;
}

} // namespace recplay
