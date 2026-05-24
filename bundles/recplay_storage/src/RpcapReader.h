// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IoBackend.h"
#include "Packet.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace recplay {

class ICodecService;
class RpcapReader final {
public:
    void SetCodecService(ICodecService* codec);
    bool Open(const std::string& path);
    void Close();
    bool SeekTo(uint64_t timestamp_ns);
    PacketPtr ReadNext();
    bool HasMore() const;
    RpcapHeader GetHeader() const;
    std::vector<ChannelInfo> GetChannels() const;
    std::vector<uint64_t> GetKeyframeTimestamps() const;

private:
    struct FooterEntry {
        uint64_t timestamp_ns = 0;
        uint64_t chunk_offset = 0;
    };

    bool ReadBytes(void* data, size_t size);
    bool LoadSchema();
    bool LoadFooter();
    bool LoadNextChunk();

    std::string path_;
    std::unique_ptr<IoBackend> backend_;
    bool open_ = false;
    bool has_more_ = false;
    RpcapHeader header_;
    std::vector<ChannelInfo> channels_;
    std::vector<FooterEntry> footer_entries_;
    std::vector<PacketPtr> current_chunk_;
    size_t current_chunk_index_ = 0;
    uint64_t next_chunk_offset_ = 0;
    ICodecService* codec_service_ = nullptr;
};

} // namespace recplay
