// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IoBackend.h"
#include "Packet.h"
#include "IStorageService.h"

#include <memory>
#include <string>
#include <cstdint>
#include <vector>

namespace recplay {

class ICodecService;
class RpcapWriter final {
public:
    void SetCodecService(ICodecService* codec);
    bool Create(const std::string& path,
                const std::vector<ChannelInfo>& channels,
                const std::string& codec);
    bool WritePacket(PacketPtr pkt);
    bool Finalize();

private:
    struct FooterEntry {
        uint64_t timestamp_ns = 0;
        uint64_t chunk_offset = 0;
    };

    bool FlushChunk(bool force_keyframe);
    bool WriteBytes(const void* data, size_t size);
    std::vector<uint8_t> BuildSchemaPayload() const;
    std::vector<uint8_t> BuildChunkPayload() const;

    std::unique_ptr<IoBackend> backend_;
    RpcapHeader header_{};
    std::string codec_;
    std::vector<ChannelInfo> channels_;
    std::vector<PacketPtr> chunk_;
    std::vector<FooterEntry> footer_entries_;
    uint64_t current_offset_ = 0;
    uint64_t current_chunk_id_ = 0;
    uint64_t first_packet_ts_ = 0;
    uint64_t last_packet_ts_ = 0;
    uint64_t packets_written_ = 0;
    ICodecService* codec_service_ = nullptr;
};

} // namespace recplay
