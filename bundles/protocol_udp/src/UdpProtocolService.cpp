// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "UdpProtocolService.h"

#if __has_include(<boost/asio.hpp>)
#include <boost/asio.hpp>
#define RECPLAY_HAS_BOOST_ASIO 1
#else
#define RECPLAY_HAS_BOOST_ASIO 0
#endif

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

namespace recplay {

std::string UdpProtocolService::GetName() const { return "UDP"; }
std::string UdpProtocolService::GetVersion() const { return "1.0.0"; }
int UdpProtocolService::GetPriority() const { return 100; }

bool UdpProtocolService::StartCapture(const std::string& configJson, PacketCallback cb) {
    try {
#if RECPLAY_HAS_BOOST_ASIO && RECPLAY_HAS_JSON
    const auto config = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
    const auto address = config.value("address", std::string("0.0.0.0"));
    const auto bind_interface = config.value("interface", std::string("0.0.0.0"));
    const auto port = static_cast<unsigned short>(config.value("port", 0));
    const auto recv_buf = config.value("recv_buf", 0);

    boost::asio::io_context io;
    boost::asio::ip::udp::socket socket(io);
    socket.open(boost::asio::ip::udp::v4());
    socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    socket.bind(boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address(bind_interface),
        port));
    if (recv_buf > 0) {
        socket.set_option(boost::asio::socket_base::receive_buffer_size(recv_buf));
    }

    const auto group_address = boost::asio::ip::make_address(address);
    if (group_address.is_multicast()) {
        socket.set_option(boost::asio::ip::multicast::join_group(group_address));
    }
#else
    (void)configJson;
#endif
    (void)cb;
    capturing_ = true;
    return true;
    } catch (...) {
        return false;
    }
}

void UdpProtocolService::StopCapture() { capturing_ = false; }
bool UdpProtocolService::IsCapturing() const { return capturing_; }
bool UdpProtocolService::StartReplay(const std::string& configJson) {
    try {
#if RECPLAY_HAS_BOOST_ASIO && RECPLAY_HAS_JSON
    const auto config = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
    const auto bind_interface = config.value("interface", std::string("0.0.0.0"));

    boost::asio::io_context io;
    boost::asio::ip::udp::socket socket(io);
    socket.open(boost::asio::ip::udp::v4());
    socket.bind(boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address(bind_interface),
        0));
#else
    (void)configJson;
#endif
    replaying_ = true;
    return true;
    } catch (...) {
        return false;
    }
}
bool UdpProtocolService::SendPacket(PacketPtr pkt) { return replaying_ && pkt != nullptr; }
void UdpProtocolService::StopReplay() { replaying_ = false; }
bool UdpProtocolService::IsReplaying() const { return replaying_; }
std::string UdpProtocolService::GetConfigSchema() const { return R"({"type":"object"})"; }
std::vector<ChannelInfo> UdpProtocolService::GetChannels() const { return {}; }

} // namespace recplay
