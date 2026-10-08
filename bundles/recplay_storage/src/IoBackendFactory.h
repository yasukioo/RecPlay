// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IoBackend.h"

#include <memory>

namespace recplay {

class IoBackendFactory {
public:
    static std::unique_ptr<IoBackend> Create();
};

} // namespace recplay
