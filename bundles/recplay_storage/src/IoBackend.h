// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace recplay {

using IoSize = std::ptrdiff_t;

class IoBackend {
public:
    virtual ~IoBackend() = default;
    virtual bool Open(const std::string& path, bool readOnly) = 0;
    virtual IoSize Write(const void* data, size_t len) = 0;
    virtual IoSize Read(void* buf, size_t len) = 0;
    virtual bool Seek(int64_t offset) = 0;
    virtual void Sync() = 0;
    virtual void Close() = 0;
    virtual void* Mmap(size_t offset, size_t length) = 0;
    virtual void Munmap(void* addr, size_t length) = 0;
};

} // namespace recplay
