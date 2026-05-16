// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "UdpProtocolService.h"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

#include <utility>

namespace recplay {

UdpProtocolService::~UdpProtocolService() {
    StopReplay();
    StopCapture();
}

std::string UdpProtocolService::GetName() const { return "UDP"; }
std::string UdpProtocolService::GetVersion() const { return "1.0.0"; }
int UdpProtocolService::GetPriority() const { return 100; }

bool UdpProtocolService::StartCapture(const std::string& configJson, PacketCallback cb) {
    RuntimeConfig config;
    if (!LoadConfig(configJson, config) || !cb) {
        return false;
    }

    StopCapture();

#if !RECPLAY_HAS_BOOST_ASIO
    (void)config;
    return false;
#else
    try {
        auto ioContext = std::make_unique<boost::asio::io_context>();
        auto socket = std::make_unique<boost::asio::ip::udp::socket>(*ioContext);

        socket->open(boost::asio::ip::udp::v4());
        socket->set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket->bind(boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address(config.bind_interface),
            config.port));
        if (config.recv_buf > 0) {
            socket->set_option(boost::asio::socket_base::receive_buffer_size(config.recv_buf));
        }

        const auto groupAddress = boost::asio::ip::make_address(config.address);
        if (groupAddress.is_multicast()) {
            socket->set_option(boost::asio::ip::multicast::join_group(groupAddress));
        }

        auto guard = std::make_unique<
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(*ioContext));

        {
            std::lock_guard<std::mutex> lock(mutex_);
            capture_config_ = config;
            capture_callback_ = std::move(cb);
            io_context_ = std::move(ioContext);
            work_guard_ = std::move(guard);
            capture_socket_ = std::move(socket);
            channels_ = {ChannelInfo{
                config.channel_id,
                config.channel_name,
                GetName(),
                config.topic,
                {
                    {"address", config.address},
                    {"interface", config.bind_interface},
                    {"port", std::to_string(config.port)}
                }
            }};
            capturing_ = true;
            sequence_.store(0, std::memory_order_release);
        }

        StartReceiveLoop();
        io_thread_ = std::thread([this] {
            if (io_context_) {
                io_context_->run();
            }
        });
        return true;
    } catch (...) {
        StopCapture();
        return false;
    }
#endif
}

void UdpProtocolService::StopCapture() {
#if RECPLAY_HAS_BOOST_ASIO
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capture_socket_) {
            boost::system::error_code ec;
            capture_socket_->cancel(ec);
            capture_socket_->close(ec);
            capture_socket_.reset();
        }
    }
    ResetIoRuntimeIfUnused();
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    capturing_ = false;
    capture_callback_ = {};
    channels_.clear();
}

bool UdpProtocolService::IsCapturing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capturing_;
}

bool UdpProtocolService::StartReplay(const std::string& configJson) {
    RuntimeConfig config;
    if (!LoadConfig(configJson, config)) {
        return false;
    }

#if !RECPLAY_HAS_BOOST_ASIO
    (void)config;
    return false;
#else
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!io_context_) {
            io_context_ = std::make_unique<boost::asio::io_context>();
            work_guard_ = std::make_unique<
                boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                boost::asio::make_work_guard(*io_context_));
            io_thread_ = std::thread([this] {
                if (io_context_) {
                    io_context_->run();
                }
            });
        }
        replay_socket_ = std::make_unique<boost::asio::ip::udp::socket>(*io_context_);
        replay_socket_->open(boost::asio::ip::udp::v4());
        replay_socket_->bind(boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address(config.bind_interface),
            0));
        replay_config_ = config;
        replaying_ = true;
        return true;
    } catch (...) {
        replaying_ = false;
        replay_socket_.reset();
        return false;
    }
#endif
}

bool UdpProtocolService::SendPacket(PacketPtr pkt) {
    if (!pkt) {
        return false;
    }

#if !RECPLAY_HAS_BOOST_ASIO
    return false;
#else
    std::lock_guard<std::mutex> lock(mutex_);
    if (!replaying_ || !replay_socket_) {
        return false;
    }

    const auto endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address(replay_config_.address),
        replay_config_.port);
    boost::system::error_code ec;
    replay_socket_->send_to(boost::asio::buffer(pkt->payload), endpoint, 0, ec);
    return !ec;
#endif
}

void UdpProtocolService::StopReplay() {
#if RECPLAY_HAS_BOOST_ASIO
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (replay_socket_) {
            boost::system::error_code ec;
            replay_socket_->cancel(ec);
            replay_socket_->close(ec);
            replay_socket_.reset();
        }
    }
    ResetIoRuntimeIfUnused();
#endif
    std::lock_guard<std::mutex> lock(mutex_);
    replaying_ = false;
}

bool UdpProtocolService::IsReplaying() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return replaying_;
}

std::string UdpProtocolService::GetConfigSchema() const {
    return R"({
  "type": "object",
  "properties": {
    "address": { "type": "string", "default": "239.1.1.1" },
    "port": { "type": "integer", "minimum": 0, "maximum": 65535, "default": 5000 },
    "interface": { "type": "string", "default": "0.0.0.0" },
    "recv_buf": { "type": "integer", "minimum": 0, "default": 8388608 },
    "channel_id": { "type": "integer", "minimum": 0, "default": 0 },
    "channel_name": { "type": "string", "default": "udp" },
    "topic": { "type": "string", "default": "" }
  },
  "required": ["address", "port"]
})";
}

std::vector<ChannelInfo> UdpProtocolService::GetChannels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_;
}

bool UdpProtocolService::LoadConfig(const std::string& configJson, RuntimeConfig& config) const {
#if !RECPLAY_HAS_JSON
    (void)configJson;
    (void)config;
    return false;
#else
    const auto json = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
    config.address = json.value("address", config.address);
    config.bind_interface = json.value("interface", config.bind_interface);
    config.port = static_cast<unsigned short>(json.value("port", static_cast<int>(config.port)));
    config.recv_buf = json.value("recv_buf", config.recv_buf);
    config.channel_id = json.value("channel_id", config.channel_id);
    config.channel_name = json.value("channel_name", config.channel_name);
    config.topic = json.value("topic", config.topic);
    return true;
#endif
}

void UdpProtocolService::StartReceiveLoop() {
#if RECPLAY_HAS_BOOST_ASIO
    if (!capture_socket_) {
        return;
    }

    capture_socket_->async_receive_from(
        boost::asio::buffer(receive_buffer_),
        remote_endpoint_,
        [this](const boost::system::error_code& error, std::size_t bytesReceived) {
            PacketCallback callback;
            RuntimeConfig config;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (error || !capturing_ || !capture_callback_) {
                    return;
                }
                callback = capture_callback_;
                config = capture_config_;
            }

            auto packet = std::make_shared<Packet>();
            packet->channel_id = config.channel_id;
            packet->topic = config.topic;
            packet->protocol_id = static_cast<uint16_t>(ProtocolId::kUDP);
            packet->sequence = sequence_.fetch_add(1, std::memory_order_acq_rel);
            packet->payload.assign(receive_buffer_.begin(), receive_buffer_.begin() + bytesReceived);
            callback(packet);

            StartReceiveLoop();
        });
#endif
}

void UdpProtocolService::ResetIoRuntimeIfUnused() {
#if RECPLAY_HAS_BOOST_ASIO
    std::thread threadToJoin;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capture_socket_ || replay_socket_) {
            return;
        }
        if (work_guard_) {
            work_guard_->reset();
        }
        if (io_context_) {
            io_context_->stop();
        }
        if (io_thread_.joinable()) {
            threadToJoin = std::move(io_thread_);
        }
        work_guard_.reset();
        io_context_.reset();
    }
    if (threadToJoin.joinable()) {
        threadToJoin.join();
    }
#endif
}

} // namespace recplay
