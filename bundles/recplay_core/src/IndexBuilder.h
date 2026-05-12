// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace recplay {

class IndexBuilder {
public:
    void AddPacket(uint64_t ts, uint32_t chunkId, uint64_t offset, uint32_t channelId);

    std::map<uint64_t, std::pair<uint32_t, uint64_t>> BuildTimeIndex() const;
    std::map<uint32_t, std::vector<uint64_t>> BuildTopicIndex() const;
    uint64_t FindNearestKeyframe(uint64_t ts) const;

private:
    struct Entry {
        uint64_t ts = 0;
        uint32_t chunkId = 0;
        uint64_t offset = 0;
        uint32_t channelId = 0;
    };

    std::vector<Entry> entries_;
};

} // namespace recplay
