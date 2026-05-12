// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "RpcapWriter.h"

#include "IoBackendFactory.h"

#include <cstring>

namespace recplay {

bool RpcapWriter::Create(const std::string& path,
                         const std::vector<ChannelInfo>& channels,
                         const std::string& codec) {
    backend_ = IoBackendFactory::Create();
    if (!backend_ || !backend_->Open(path, false)) {
        return false;
    }
    codec_ = codec;
    channels_ = channels;
    chunk_.clear();
    return true;
}

bool RpcapWriter::WritePacket(PacketPtr pkt) {
    if (!backend_ || !pkt) {
        return false;
    }
    chunk_.push_back(pkt);
    return true;
}

bool RpcapWriter::Finalize() {
    if (backend_) {
        backend_->Sync();
        backend_->Close();
    }
    return true;
}

} // namespace recplay
