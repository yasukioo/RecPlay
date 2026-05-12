// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "TcpProtocolService.h"

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

std::string TcpProtocolService::GetName() const { return "TCP"; }
std::string TcpProtocolService::GetVersion() const { return "1.0.0"; }
int TcpProtocolService::GetPriority() const { return 90; }
bool TcpProtocolService::StartCapture(const std::string& configJson, PacketCallback cb) {
    try {
#if RECPLAY_HAS_BOOST_ASIO && RECPLAY_HAS_JSON
    const auto config = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
    const auto mode = config.value("mode", std::string("server"));
    const auto host = config.value("host", std::string("0.0.0.0"));
    const auto port = static_cast<unsigned short>(config.value("port", 0));

    boost::asio::io_context io;
    if (mode == "server") {
        boost::asio::ip::tcp::acceptor acceptor(
            io,
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address(host),
                port));
        (void)acceptor;
    } else {
        boost::asio::ip::tcp::socket socket(io);
        socket.connect(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address(host),
            port));
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
void TcpProtocolService::StopCapture() { capturing_ = false; }
bool TcpProtocolService::IsCapturing() const { return capturing_; }
bool TcpProtocolService::StartReplay(const std::string& configJson) {
    try {
#if RECPLAY_HAS_BOOST_ASIO && RECPLAY_HAS_JSON
    const auto config = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
    const auto host = config.value("host", std::string("127.0.0.1"));
    const auto port = static_cast<unsigned short>(config.value("port", 0));

    boost::asio::io_context io;
    boost::asio::ip::tcp::socket socket(io);
    socket.connect(boost::asio::ip::tcp::endpoint(
        boost::asio::ip::make_address(host),
        port));
#else
    (void)configJson;
#endif
    replaying_ = true;
    return true;
    } catch (...) {
        return false;
    }
}
bool TcpProtocolService::SendPacket(PacketPtr pkt) { return replaying_ && pkt != nullptr; }
void TcpProtocolService::StopReplay() { replaying_ = false; }
bool TcpProtocolService::IsReplaying() const { return replaying_; }
std::string TcpProtocolService::GetConfigSchema() const { return R"({"type":"object"})"; }
std::vector<ChannelInfo> TcpProtocolService::GetChannels() const { return {}; }

} // namespace recplay
