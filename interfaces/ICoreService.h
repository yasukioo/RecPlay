// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace recplay {

struct CoreTopicMapping {
    uint32_t source_channel_id = 0;
    uint32_t target_channel_id = 0;
    std::string target_topic;
};

class ICoreService {
public:
    virtual ~ICoreService() = default;

    virtual bool StartProtocolCapture(const std::string& name,
                                      const std::string& configJson) = 0;
    virtual void StopProtocolCapture(const std::string& name) = 0;
    virtual void SetTopicMappings(const std::vector<CoreTopicMapping>& mappings) = 0;
};

} // namespace recplay
