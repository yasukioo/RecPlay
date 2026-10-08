// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IStorageService.h"
#include "RpcapReader.h"
#include "RpcapWriter.h"

#include <atomic>
#include <mutex>
#include <string>

namespace recplay {

class ICodecService;
class StorageService final : public IStorageService {
public:
    void SetCodecService(ICodecService* codec);
    bool CreateFile(const std::string& path,
                    const std::vector<ChannelInfo>& channels,
                    const std::string& codec = "zstd") override;
    bool WritePacket(PacketPtr pkt) override;
    bool FinalizeFile() override;

    bool OpenFile(const std::string& path) override;
    void CloseFile() override;
    RpcapHeader GetHeader() const override;
    std::vector<ChannelInfo> GetChannels() const override;

    bool SeekTo(uint64_t timestamp_ns) override;
    PacketPtr ReadNext() override;
    bool HasMore() const override;
    std::vector<uint64_t> GetKeyframeTimestamps() const override;
    std::optional<RecordingFileInfo> ProbeFile(const std::string& path) const override;
    std::vector<uint64_t> GetDensity(uint32_t buckets) const override;

    bool IsWriting() const override;
    bool IsReading() const override;

private:
    RpcapWriter writer_;
    RpcapReader reader_;
    std::atomic<bool> writing_{false};
    std::atomic<bool> reading_{false};
    mutable std::mutex path_mutex_;
    std::string current_path_;
};

} // namespace recplay
