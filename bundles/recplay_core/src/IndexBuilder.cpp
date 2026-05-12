// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IndexBuilder.h"

#include <algorithm>

namespace recplay {

void IndexBuilder::AddPacket(uint64_t ts, uint32_t chunkId, uint64_t offset, uint32_t channelId) {
    entries_.push_back(Entry{ts, chunkId, offset, channelId});
}

std::map<uint64_t, std::pair<uint32_t, uint64_t>> IndexBuilder::BuildTimeIndex() const {
    std::map<uint64_t, std::pair<uint32_t, uint64_t>> index;
    for (const auto& entry : entries_) {
        index[entry.ts] = {entry.chunkId, entry.offset};
    }
    return index;
}

std::map<uint32_t, std::vector<uint64_t>> IndexBuilder::BuildTopicIndex() const {
    std::map<uint32_t, std::vector<uint64_t>> index;
    for (const auto& entry : entries_) {
        index[entry.channelId].push_back(entry.offset);
    }
    for (auto& [_, offsets] : index) {
        std::reverse(offsets.begin(), offsets.end());
    }
    return index;
}

uint64_t IndexBuilder::FindNearestKeyframe(uint64_t ts) const {
    if (entries_.empty()) {
        return 0;
    }

    uint64_t best = entries_.front().ts;
    for (const auto& entry : entries_) {
        if (entry.ts <= ts) {
            best = entry.ts;
        }
    }
    return best;
}

} // namespace recplay
