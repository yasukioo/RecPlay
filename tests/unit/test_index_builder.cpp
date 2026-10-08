// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IndexBuilder.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using recplay::IndexBuilder;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestBuildTimeIndexSortsEntriesByTimestamp() {
    IndexBuilder builder;
    builder.AddPacket(300, 2, 30, 9);
    builder.AddPacket(100, 1, 10, 7);
    builder.AddPacket(200, 1, 20, 7);
    builder.AddPacket(150, 3, 15, 8);

    const auto time_index = builder.BuildTimeIndex();
    Expect(time_index.size() == 4, "time index should contain every packet with a unique timestamp");

    auto it = time_index.begin();
    Expect(it->first == 100 && it->second.first == 1 && it->second.second == 10,
           "first time index entry should be the earliest packet");
    ++it;
    Expect(it->first == 150 && it->second.first == 3 && it->second.second == 15,
           "second time index entry should preserve timestamp ordering");
    ++it;
    Expect(it->first == 200 && it->second.first == 1 && it->second.second == 20,
           "third time index entry should preserve timestamp ordering");
    ++it;
    Expect(it->first == 300 && it->second.first == 2 && it->second.second == 30,
           "last time index entry should be the latest packet");
}

void TestBuildTopicIndexGroupsOffsetsPerChannelInTimestampOrder() {
    IndexBuilder builder;
    builder.AddPacket(300, 2, 30, 7);
    builder.AddPacket(100, 1, 10, 7);
    builder.AddPacket(200, 1, 20, 8);
    builder.AddPacket(150, 3, 15, 7);

    const auto topic_index = builder.BuildTopicIndex();
    Expect(topic_index.size() == 2, "topic index should group entries by channel id");
    Expect(topic_index.at(7) == std::vector<uint64_t>({10, 15, 30}),
           "channel 7 offsets should be sorted by packet timestamp");
    Expect(topic_index.at(8) == std::vector<uint64_t>({20}),
           "channel 8 offsets should be preserved");
}

void TestFindNearestKeyframeHonorsExplicitAndImplicitChunkStarts() {
    IndexBuilder builder;
    builder.AddPacket(100, 1, 10, 7, false);  // auto-keyframe: first packet in chunk 1
    builder.AddPacket(140, 1, 14, 7, false);
    builder.AddPacket(180, 2, 18, 7, false);  // auto-keyframe: first packet in chunk 2
    builder.AddPacket(220, 2, 22, 7, true);   // explicit keyframe in same chunk
    builder.AddPacket(260, 3, 26, 7, false);  // auto-keyframe: first packet in chunk 3

    Expect(builder.FindNearestKeyframe(50) == 10,
           "querying before the first packet should return the earliest keyframe");
    Expect(builder.FindNearestKeyframe(170) == 10,
           "query before the second chunk should resolve to the first chunk's keyframe");
    Expect(builder.FindNearestKeyframe(200) == 18,
           "first packet in a new chunk should be treated as a seekable keyframe");
    Expect(builder.FindNearestKeyframe(240) == 22,
           "explicit keyframes inside a chunk should be queryable");
    Expect(builder.FindNearestKeyframe(999) == 26,
           "queries after the final packet should return the newest keyframe");
}

} // namespace

int main() {
    try {
        TestBuildTimeIndexSortsEntriesByTimestamp();
        TestBuildTopicIndexGroupsOffsetsPerChannelInTimestampOrder();
        TestFindNearestKeyframeHonorsExplicitAndImplicitChunkStarts();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
