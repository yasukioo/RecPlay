// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "TcpProtocolService.h"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

namespace recplay {

TcpProtocolService::~TcpProtocolService() {
    StopReplay();
    StopCapture();
}

std::string TcpProtocolService::GetName() const { return "TCP"; }
std::string TcpProtocolService::GetVersion() const { return "1.0.0"; }
int TcpProtocolService::GetPriority() const { return 90; }

bool TcpProtocolService::StartCapture(const std::string& configJson, PacketCallback cb) {
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
        auto guard = std::make_unique<
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(*ioContext));

        {
            std::lock_guard<std::mutex> lock(mutex_);
            io_context_ = std::move(ioContext);
            work_guard_ = std::move(guard);
            capture_callback_ = std::move(cb);
            capture_config_ = config;
            channels_ = {ChannelInfo{
                config.channel_id,
                config.channel_name,
                GetName(),
                config.topic,
                {
                    {"mode", config.mode},
                    {"host", config.host},
                    {"port", std::to_string(config.port)}
                }
            }};
            capturing_ = true;
            sequence_.store(0, std::memory_order_release);
        }

        if (config.mode == "server") {
            acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
                *io_context_,
                boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address(config.host),
                    config.port));
            capture_socket_ = std::make_unique<boost::asio::ip::tcp::socket>(*io_context_);
            StartAcceptLoop();
        } else {
            capture_socket_ = std::make_unique<boost::asio::ip::tcp::socket>(*io_context_);
            capture_socket_->connect(boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address(config.host),
                config.port));
            StartReadLoop();
        }

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

void TcpProtocolService::StopCapture() {
#if RECPLAY_HAS_BOOST_ASIO
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (acceptor_) {
            boost::system::error_code ec;
            acceptor_->cancel(ec);
            acceptor_->close(ec);
            acceptor_.reset();
        }
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

bool TcpProtocolService::IsCapturing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capturing_;
}

bool TcpProtocolService::StartReplay(const std::string& configJson) {
    RuntimeConfig config;
    if (!LoadConfig(configJson, config)) {
        return false;
    }

#if !RECPLAY_HAS_BOOST_ASIO
    (void)config;
    return false;
#else
    try {
        std::unique_ptr<boost::asio::ip::tcp::socket> socket;
        {
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

            socket = std::make_unique<boost::asio::ip::tcp::socket>(*io_context_);
        }

        socket->connect(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address(config.host),
            config.port));

        std::lock_guard<std::mutex> lock(mutex_);
        if (!io_context_) {
            return false;
        }

        replay_socket_ = std::move(socket);
        replay_config_ = config;
        replaying_ = true;
        return true;
    } catch (...) {
        std::lock_guard<std::mutex> lock(mutex_);
        replay_socket_.reset();
        replaying_ = false;
        return false;
    }
#endif
}

bool TcpProtocolService::SendPacket(PacketPtr pkt) {
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

    boost::system::error_code ec;
    boost::asio::write(*replay_socket_, boost::asio::buffer(pkt->payload), ec);
    return !ec;
#endif
}

void TcpProtocolService::StopReplay() {
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

bool TcpProtocolService::IsReplaying() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return replaying_;
}

std::string TcpProtocolService::GetConfigSchema() const {
    return R"({
  "type": "object",
  "properties": {
    "mode": { "type": "string", "enum": ["server", "client"], "default": "server" },
    "host": { "type": "string", "default": "0.0.0.0" },
    "port": { "type": "integer", "minimum": 0, "maximum": 65535, "default": 7400 },
    "channel_id": { "type": "integer", "minimum": 0, "default": 0 },
    "channel_name": { "type": "string", "default": "tcp" },
    "topic": { "type": "string", "default": "" }
  },
  "required": ["mode", "host", "port"]
})";
}

std::vector<ChannelInfo> TcpProtocolService::GetChannels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_;
}

bool TcpProtocolService::LoadConfig(const std::string& configJson, RuntimeConfig& config) const {
#if !RECPLAY_HAS_JSON
    (void)configJson;
    (void)config;
    return false;
#else
    const auto json = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
    config.mode = json.value("mode", config.mode);
    config.host = json.value("host", config.host);
    config.port = static_cast<unsigned short>(json.value("port", static_cast<int>(config.port)));
    config.channel_id = json.value("channel_id", config.channel_id);
    config.channel_name = json.value("channel_name", config.channel_name);
    config.topic = json.value("topic", config.topic);
    return true;
#endif
}

void TcpProtocolService::StartAcceptLoop() {
#if RECPLAY_HAS_BOOST_ASIO
    if (!acceptor_ || !capture_socket_) {
        return;
    }

    acceptor_->async_accept(
        *capture_socket_,
        [this](const boost::system::error_code& error) {
            if (error) {
                return;
            }
            StartReadLoop();
        });
#endif
}

void TcpProtocolService::StartReadLoop() {
#if RECPLAY_HAS_BOOST_ASIO
    if (!capture_socket_) {
        return;
    }

    capture_socket_->async_read_some(
        boost::asio::buffer(receive_buffer_),
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
            packet->protocol_id = static_cast<uint16_t>(ProtocolId::kTCP);
            packet->sequence = sequence_.fetch_add(1, std::memory_order_acq_rel);
            packet->payload.assign(receive_buffer_.begin(), receive_buffer_.begin() + bytesReceived);
            callback(packet);

            StartReadLoop();
        });
#endif
}

void TcpProtocolService::ResetIoRuntimeIfUnused() {
#if RECPLAY_HAS_BOOST_ASIO
    std::thread threadToJoin;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (acceptor_ || capture_socket_ || replay_socket_) {
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
