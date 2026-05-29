// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RingBuffer.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestPushPopMaintainsOrderAndSize() {
    recplay::SPSCRingBuffer<int, 8> buffer;

    Expect(buffer.IsEmpty(), "ring buffer should start empty");
    Expect(buffer.GetCapacity() == 7, "reported capacity should match usable slot count");
    Expect(buffer.TryPush(1), "first push should succeed");
    Expect(buffer.TryPush(2), "second push should succeed");
    Expect(buffer.TryPush(3), "third push should succeed");
    Expect(buffer.Size() == 3, "size should track pushed items");

    const auto first = buffer.TryPop();
    const auto second = buffer.TryPop();
    const auto third = buffer.TryPop();
    Expect(first.has_value() && *first == 1, "first pop should return the oldest item");
    Expect(second.has_value() && *second == 2, "second pop should preserve FIFO order");
    Expect(third.has_value() && *third == 3, "third pop should preserve FIFO order");
    Expect(!buffer.TryPop().has_value(), "popping an empty buffer should return nullopt");
    Expect(buffer.IsEmpty(), "buffer should be empty after popping every item");
}

void TestFullAndEmptyBoundariesReserveOneSlot() {
    recplay::SPSCRingBuffer<int, 4> buffer;

    Expect(buffer.TryPush(10), "first boundary push should succeed");
    Expect(buffer.TryPush(20), "second boundary push should succeed");
    Expect(buffer.TryPush(30), "third boundary push should succeed");
    Expect(!buffer.TryPush(40), "buffer should reject pushes once it reaches its usable capacity");
    Expect(buffer.IsFull(), "buffer should report full after filling usable slots");
    Expect(buffer.GetCapacity() == 3, "capacity should report usable slots rather than backing array size");
    Expect(buffer.Size() == 3, "full buffer should expose usable slot count");

    const auto first = buffer.TryPop();
    Expect(first.has_value() && *first == 10, "full buffer should still pop the oldest item first");
    Expect(buffer.TryPush(40), "push should succeed again after one item is popped");

    const auto second = buffer.TryPop();
    const auto third = buffer.TryPop();
    const auto fourth = buffer.TryPop();
    Expect(second.has_value() && *second == 20, "second value should survive wrap-around");
    Expect(third.has_value() && *third == 30, "third value should survive wrap-around");
    Expect(fourth.has_value() && *fourth == 40, "newly pushed value should be readable after wrap-around");
    Expect(!buffer.TryPop().has_value(), "buffer should become empty again after wrap-around reads");
}

void TestSingleProducerSingleConsumerTransferPreservesOrder() {
    constexpr size_t kMessageCount = 10'000;
    recplay::SPSCRingBuffer<size_t, 1024> buffer;
    std::vector<size_t> consumed(kMessageCount, 0);

    std::thread producer([&] {
        for (size_t value = 0; value < kMessageCount; ++value) {
            while (!buffer.TryPush(value)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        size_t index = 0;
        while (index < kMessageCount) {
            const auto item = buffer.TryPop();
            if (!item.has_value()) {
                std::this_thread::yield();
                continue;
            }
            consumed[index++] = *item;
        }
    });

    producer.join();
    consumer.join();

    for (size_t i = 0; i < kMessageCount; ++i) {
        Expect(consumed[i] == i, "SPSC transfer should preserve FIFO order at index " + std::to_string(i));
    }
    Expect(buffer.IsEmpty(), "threaded transfer should drain the queue completely");
}

} // namespace

int main() {
    try {
        TestPushPopMaintainsOrderAndSize();
        TestFullAndEmptyBoundariesReserveOneSlot();
        TestSingleProducerSingleConsumerTransferPreservesOrder();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
