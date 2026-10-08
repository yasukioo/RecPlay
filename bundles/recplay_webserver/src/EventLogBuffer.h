// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace recplay {

struct EventLogEntry {
    uint64_t timestamp_ms = 0;
    std::string level;
    std::string message;
    std::string source;
};

class EventLogBuffer {
public:
    explicit EventLogBuffer(std::size_t capacity = 256);

    void Push(EventLogEntry entry);
    std::vector<EventLogEntry> Snapshot() const;
    void Clear();

private:
    std::size_t capacity_ = 0;
    mutable std::mutex mutex_;
    std::vector<EventLogEntry> entries_;
};

} // namespace recplay
