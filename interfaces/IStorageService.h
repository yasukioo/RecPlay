// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "Packet.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifdef CreateFile
#undef CreateFile
#endif

namespace recplay {

// Read-only summary of a .rpcap file, returned by ProbeFile without touching any
// in-progress recording/playback. Keeps the on-disk format encapsulated behind
// the service interface so API/UI layers never link the storage implementation.
struct RecordingFileInfo {
    uint64_t duration_ns = 0;
    uint64_t total_packets = 0;
    uint64_t creation_time = 0;
    uint32_t channel_count = 0;
};

class IStorageService {
public:
    virtual ~IStorageService() = default;

    virtual bool CreateFile(const std::string& path,
                            const std::vector<ChannelInfo>& channels,
                            const std::string& codec = "zstd") = 0;
    virtual bool WritePacket(PacketPtr pkt) = 0;
    virtual bool FinalizeFile() = 0;

    virtual bool OpenFile(const std::string& path) = 0;
    virtual void CloseFile() = 0;
    virtual RpcapHeader GetHeader() const = 0;
    virtual std::vector<ChannelInfo> GetChannels() const = 0;

    virtual bool SeekTo(uint64_t timestamp_ns) = 0;
    virtual PacketPtr ReadNext() = 0;
    virtual bool HasMore() const = 0;
    virtual std::vector<uint64_t> GetKeyframeTimestamps() const = 0;

    // Probe an arbitrary .rpcap file's metadata using a transient reader, so it
    // is safe to call while another file is open for recording/playback.
    // Returns nullopt when the file can't be opened or parsed.
    virtual std::optional<RecordingFileInfo> ProbeFile(const std::string& path) const = 0;

    // Event density of the currently-open playback file: total packet counts
    // distributed across `buckets` equal time slices over the file duration.
    // Empty when no file is open. Cheap (reads chunk headers only).
    virtual std::vector<uint64_t> GetDensity(uint32_t buckets) const = 0;

    virtual bool IsWriting() const = 0;
    virtual bool IsReading() const = 0;
};

} // namespace recplay
