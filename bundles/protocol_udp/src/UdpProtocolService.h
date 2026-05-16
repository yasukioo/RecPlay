// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "IProtocolService.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if __has_include(<boost/asio.hpp>)
#include <boost/asio.hpp>
#define RECPLAY_HAS_BOOST_ASIO 1
#else
#define RECPLAY_HAS_BOOST_ASIO 0
#endif

namespace recplay {

class UdpProtocolService final : public IProtocolService {
public:
    ~UdpProtocolService() override;

    std::string GetName() const override;
    std::string GetVersion() const override;
    int GetPriority() const override;
    bool StartCapture(const std::string& configJson, PacketCallback cb) override;
    void StopCapture() override;
    bool IsCapturing() const override;
    bool StartReplay(const std::string& configJson) override;
    bool SendPacket(PacketPtr pkt) override;
    void StopReplay() override;
    bool IsReplaying() const override;
    std::string GetConfigSchema() const override;
    std::vector<ChannelInfo> GetChannels() const override;

private:
    struct RuntimeConfig {
        std::string address = "0.0.0.0";
        std::string bind_interface = "0.0.0.0";
        unsigned short port = 0;
        int recv_buf = 0;
        uint32_t channel_id = 0;
        std::string channel_name = "udp";
        std::string topic;
    };

    bool LoadConfig(const std::string& configJson, RuntimeConfig& config) const;
    void StartReceiveLoop();
    void ResetIoRuntimeIfUnused();

    mutable std::mutex mutex_;
    bool capturing_ = false;
    bool replaying_ = false;
    std::vector<ChannelInfo> channels_;
    PacketCallback capture_callback_;
    RuntimeConfig capture_config_;
    RuntimeConfig replay_config_;

#if RECPLAY_HAS_BOOST_ASIO
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::unique_ptr<boost::asio::ip::udp::socket> capture_socket_;
    std::unique_ptr<boost::asio::ip::udp::socket> replay_socket_;
    boost::asio::ip::udp::endpoint remote_endpoint_;
    std::array<uint8_t, 65536> receive_buffer_{};
    std::thread io_thread_;
    std::atomic<uint32_t> sequence_{0};
#endif
};

} // namespace recplay
