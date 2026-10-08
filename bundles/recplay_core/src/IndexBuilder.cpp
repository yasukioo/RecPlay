// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IndexBuilder.h"

#include <algorithm>

namespace recplay {

void IndexBuilder::AddPacket(
    uint64_t ts,
    uint32_t chunkId,
    uint64_t offset,
    uint32_t channelId,
    bool isKeyframe) {
    const bool is_first_in_chunk = std::none_of(
        entries_.begin(),
        entries_.end(),
        [chunkId](const Entry& entry) {
            return entry.chunkId == chunkId;
        });
    entries_.push_back(Entry{ts, chunkId, offset, channelId, isKeyframe || is_first_in_chunk});
}

std::map<uint64_t, std::pair<uint32_t, uint64_t>> IndexBuilder::BuildTimeIndex() const {
    std::map<uint64_t, std::pair<uint32_t, uint64_t>> index;
    std::vector<Entry> sorted_entries = entries_;
    std::sort(
        sorted_entries.begin(),
        sorted_entries.end(),
        [](const Entry& lhs, const Entry& rhs) {
            if (lhs.ts != rhs.ts) {
                return lhs.ts < rhs.ts;
            }
            if (lhs.chunkId != rhs.chunkId) {
                return lhs.chunkId < rhs.chunkId;
            }
            return lhs.offset < rhs.offset;
        });

    for (const auto& entry : sorted_entries) {
        index[entry.ts] = {entry.chunkId, entry.offset};
    }
    return index;
}

std::map<uint32_t, std::vector<uint64_t>> IndexBuilder::BuildTopicIndex() const {
    std::map<uint32_t, std::vector<uint64_t>> index;
    std::vector<Entry> sorted_entries = entries_;
    std::sort(
        sorted_entries.begin(),
        sorted_entries.end(),
        [](const Entry& lhs, const Entry& rhs) {
            if (lhs.channelId != rhs.channelId) {
                return lhs.channelId < rhs.channelId;
            }
            if (lhs.ts != rhs.ts) {
                return lhs.ts < rhs.ts;
            }
            return lhs.offset < rhs.offset;
        });

    for (const auto& entry : sorted_entries) {
        index[entry.channelId].push_back(entry.offset);
    }
    return index;
}

uint64_t IndexBuilder::FindNearestKeyframe(uint64_t ts) const {
    if (entries_.empty()) {
        return 0;
    }

    std::vector<Entry> keyframes;
    keyframes.reserve(entries_.size());
    for (const auto& entry : entries_) {
        if (entry.isKeyframe) {
            keyframes.push_back(entry);
        }
    }

    if (keyframes.empty()) {
        return 0;
    }

    std::sort(
        keyframes.begin(),
        keyframes.end(),
        [](const Entry& lhs, const Entry& rhs) {
            if (lhs.ts != rhs.ts) {
                return lhs.ts < rhs.ts;
            }
            if (lhs.chunkId != rhs.chunkId) {
                return lhs.chunkId < rhs.chunkId;
            }
            return lhs.offset < rhs.offset;
        });

    uint64_t best_offset = keyframes.front().offset;
    for (const auto& entry : keyframes) {
        if (entry.ts > ts) {
            break;
        }
        best_offset = entry.offset;
    }
    return best_offset;
}

} // namespace recplay
