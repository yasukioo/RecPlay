// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <utility>

namespace recplay {

// SPSC (Single-Producer Single-Consumer) ring buffer.
// Capacity must be a power of 2.
template <typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

public:
    // Try to push an item. Returns false when the buffer is full.
    bool TryPush(const T& item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Try to push an item by move. Returns false when the buffer is full.
    bool TryPush(T&& item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Try to pop an item. Returns nullopt when the buffer is empty.
    std::optional<T> TryPop() {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        auto item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return item;
    }

    // Approximate number of items currently in the queue.
    size_t Size() const {
        const auto head = head_.load(std::memory_order_acquire);
        const auto tail = tail_.load(std::memory_order_acquire);
        return (head - tail + Capacity) & kMask;
    }

    size_t GetCapacity() const { return Capacity; }

    bool IsEmpty() const { return Size() == 0; }
    bool IsFull() const { return Size() == Capacity - 1; }

private:
    static constexpr size_t kMask = Capacity - 1;

    // Separate producer and consumer indices to reduce false sharing.
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::array<T, Capacity> buffer_{};
};

} // namespace recplay
