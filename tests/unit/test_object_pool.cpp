// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "ObjectPool.h"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

struct Widget {
    int value = 0;
};

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestAcquireAndReleaseReuseExistingObjects() {
    recplay::ObjectPool<Widget> pool(1);

    auto first = pool.Acquire();
    Expect(first != nullptr, "pool should hand out an object");
    auto* first_address = first.get();
    first->value = 42;

    pool.Release(std::move(first));
    auto second = pool.Acquire();
    Expect(second != nullptr, "released object should be acquirable again");
    Expect(second.get() == first_address, "pool should reuse released objects before allocating new ones");
}

void TestPoolExpandsWhenExhaustedAndIgnoresNullRelease() {
    recplay::ObjectPool<Widget> pool(0);

    auto first = pool.Acquire();
    auto second = pool.Acquire();
    Expect(first != nullptr, "pool should allocate a new object when empty");
    Expect(second != nullptr, "pool should keep allocating while exhausted");
    Expect(first.get() != second.get(), "exhausted pool should return distinct allocations");

    std::unique_ptr<Widget> null_widget;
    pool.Release(std::move(null_widget));
    pool.Release(std::move(first));
    pool.Release(std::move(second));

    auto reused_one = pool.Acquire();
    auto reused_two = pool.Acquire();
    Expect(reused_one != nullptr && reused_two != nullptr,
           "objects released after exhaustion should be reusable");
    Expect(reused_one.get() != reused_two.get(),
           "pool should preserve separate released objects for later reuse");
}

} // namespace

int main() {
    try {
        TestAcquireAndReleaseReuseExistingObjects();
        TestPoolExpandsWhenExhaustedAndIgnoresNullRelease();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
