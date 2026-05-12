// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace recplay {

/// SPSC (Single-Producer Single-Consumer) 无锁环形缓冲区
/// Capacity 必须是 2 的幂
template <typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

public:
    /// 尝试入队 (生产者调用)，满则返回 false
    bool TryPush(const T& item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // 满
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// 尝试入队 (移动语义)
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

    /// 尝试出队 (消费者调用)，空则返回 nullopt
    std::optional<T> TryPop() {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;  // 空
        }
        auto item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return item;
    }

    /// 当前队列中元素数量 (近似值，用于统计)
    size_t Size() const {
        const auto h = head_.load(std::memory_order_acquire);
        const auto t = tail_.load(std::memory_order_acquire);
        return (h - t + Capacity) & kMask;
    }

    size_t GetCapacity() const { return Capacity; }

    bool IsEmpty() const { return Size() == 0; }
    bool IsFull() const { return Size() == Capacity - 1; }

private:
    static constexpr size_t kMask = Capacity - 1;

    // 分离到不同缓存行，避免 false sharing
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::array<T, Capacity> buffer_;
};

} // namespace recplay
