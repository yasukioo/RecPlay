// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace recplay {

template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initialSize = 65536) {
        pool_.reserve(initialSize);
        for (size_t i = 0; i < initialSize; ++i) {
            pool_.push_back(std::make_unique<T>());
        }
    }

    std::unique_ptr<T> Acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty()) {
            return std::make_unique<T>();
        }
        auto value = std::move(pool_.back());
        pool_.pop_back();
        return value;
    }

    void Release(std::unique_ptr<T> value) {
        if (!value) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(value));
    }

private:
    std::mutex mutex_;
    std::vector<std::unique_ptr<T>> pool_;
};

} // namespace recplay
