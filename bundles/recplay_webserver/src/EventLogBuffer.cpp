// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "EventLogBuffer.h"

#include <utility>

namespace recplay {

EventLogBuffer::EventLogBuffer(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

void EventLogBuffer::Push(EventLogEntry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::move(entry));
    if (entries_.size() > capacity_) {
        entries_.erase(entries_.begin());
    }
}

std::vector<EventLogEntry> EventLogBuffer::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

void EventLogBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

} // namespace recplay
