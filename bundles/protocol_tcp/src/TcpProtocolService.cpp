// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "TcpProtocolService.h"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

#include <algorithm>
#include <chrono>

namespace recplay {

namespace {

uint64_t GetSteadyClockNanoseconds() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t ComputeRelativeCaptureTimestamp(uint64_t captureEpochNs) {
    const auto nowNs = GetSteadyClockNanoseconds();
    if (captureEpochNs == 0 || nowNs <= captureEpochNs) {
        return 1;
    }
    return nowNs - captureEpochNs;
}

} // namespace

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
        std::unique_ptr<boost::asio::io_context> createdIoContext;
        std::unique_ptr<
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> createdGuard;
        boost::asio::io_context* activeIoContext = nullptr;
        bool startIoThread = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (io_context_) {
                activeIoContext = io_context_.get();
            } else {
                createdIoContext = std::make_unique<boost::asio::io_context>();
                activeIoContext = createdIoContext.get();
                createdGuard = std::make_unique<
                    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                    boost::asio::make_work_guard(*activeIoContext));
                startIoThread = true;
            }
        }

        if (activeIoContext == nullptr) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (createdIoContext) {
                io_context_ = std::move(createdIoContext);
            }
            if (createdGuard) {
                work_guard_ = std::move(createdGuard);
            }
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
            capture_epoch_ns_.store(GetSteadyClockNanoseconds(), std::memory_order_release);
        }

        if (startIoThread) {
            io_thread_ = std::thread([activeIoContext] {
                if (activeIoContext) {
                    activeIoContext->run();
                }
            });
        }

        if (config.mode == "server") {
            acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
                *activeIoContext,
                boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address(config.host),
                    config.port));
            StartAcceptLoop();
        } else {
            auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*activeIoContext);
            auto buffer = std::make_shared<std::array<uint8_t, 65536>>();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                capture_sockets_.push_back(socket);
            }
            socket->async_connect(
                boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address(config.host),
                    config.port),
                [this, socket, buffer](const boost::system::error_code& error) {
                    if (error) {
                        RemoveCaptureSocket(socket);
                        return;
                    }
                    StartReadLoop(socket, buffer);
                });
        }

        return true;
    } catch (...) {
        StopCapture();
        return false;
    }
#endif
}

void TcpProtocolService::StopCapture() {
#if RECPLAY_HAS_BOOST_ASIO
    std::vector<std::shared_ptr<boost::asio::ip::tcp::socket>> socketsToClose;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capturing_ = false;
        capture_callback_ = {};
        channels_.clear();
        if (acceptor_) {
            boost::system::error_code ec;
            acceptor_->cancel(ec);
            acceptor_->close(ec);
            acceptor_.reset();
        }
        socketsToClose = capture_sockets_;
        capture_sockets_.clear();
    }
    for (const auto& socket : socketsToClose) {
        if (!socket) {
            continue;
        }
        boost::system::error_code ec;
        socket->cancel(ec);
        socket->close(ec);
    }
    ResetIoRuntimeIfUnused();
    std::lock_guard<std::mutex> lock(mutex_);
    capture_epoch_ns_.store(0, std::memory_order_release);
#endif
}

bool TcpProtocolService::IsCapturing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capturing_;
}

bool TcpProtocolService::StartReplay(const std::string& configJson) {
    std::vector<RuntimeConfig> configs;
    if (!LoadReplayConfigs(configJson, configs) || configs.empty()) {
        return false;
    }

#if !RECPLAY_HAS_BOOST_ASIO
    (void)configs;
    return false;
#else
    try {
        StopReplay();
        boost::asio::io_context* activeIoContext = nullptr;
        bool startIoThread = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!io_context_) {
                io_context_ = std::make_unique<boost::asio::io_context>();
                work_guard_ = std::make_unique<
                    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                    boost::asio::make_work_guard(*io_context_));
                startIoThread = true;
            }
            activeIoContext = io_context_.get();
        }

        if (activeIoContext == nullptr) {
            return false;
        }

        if (startIoThread) {
            io_thread_ = std::thread([activeIoContext] {
                if (activeIoContext) {
                    activeIoContext->run();
                }
            });
        }

        std::vector<std::unique_ptr<boost::asio::ip::tcp::socket>> sockets;
        sockets.reserve(configs.size());
        for (const auto& config : configs) {
            auto socket = std::make_unique<boost::asio::ip::tcp::socket>(*activeIoContext);
            socket->connect(boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address(config.host),
                config.port));
            sockets.push_back(std::move(socket));
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!io_context_) {
            return false;
        }

        replay_sockets_ = std::move(sockets);
        replay_configs_ = std::move(configs);
        replaying_ = !replay_sockets_.empty();
        return replaying_;
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            replay_sockets_.clear();
            replay_configs_.clear();
            replaying_ = false;
        }
        ResetIoRuntimeIfUnused();
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
    if (!replaying_ || replay_sockets_.empty()) {
        return false;
    }

    bool sentAny = false;
    for (auto& socket : replay_sockets_) {
        if (!socket) {
            continue;
        }
        boost::system::error_code ec;
        boost::asio::write(*socket, boost::asio::buffer(pkt->payload), ec);
        if (ec) {
            return false;
        }
        sentAny = true;
    }
    return sentAny;
#endif
}

void TcpProtocolService::StopReplay() {
#if RECPLAY_HAS_BOOST_ASIO
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& socket : replay_sockets_) {
            if (!socket) {
                continue;
            }
            boost::system::error_code ec;
            socket->cancel(ec);
            socket->close(ec);
        }
        replay_sockets_.clear();
    }
    ResetIoRuntimeIfUnused();
#endif
    std::lock_guard<std::mutex> lock(mutex_);
    replaying_ = false;
    replay_configs_.clear();
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
    try {
        const auto json = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
        config.mode = json.value("mode", config.mode);
        config.host = json.value("host", config.host);
        config.port = static_cast<unsigned short>(json.value("port", static_cast<int>(config.port)));
        config.channel_id = json.value("channel_id", config.channel_id);
        config.channel_name = json.value("channel_name", config.channel_name);
        config.topic = json.value("topic", config.topic);
        return true;
    } catch (...) {
        return false;
    }
#endif
}

bool TcpProtocolService::LoadReplayConfigs(const std::string& configJson, std::vector<RuntimeConfig>& configs) const {
#if !RECPLAY_HAS_JSON
    (void)configJson;
    (void)configs;
    return false;
#else
    try {
        const auto json = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
        configs.clear();
        if (json.is_array()) {
            for (const auto& item : json) {
                RuntimeConfig config;
                config.mode = item.value("mode", config.mode);
                config.host = item.value("host", config.host);
                config.port = static_cast<unsigned short>(item.value("port", static_cast<int>(config.port)));
                config.channel_id = item.value("channel_id", config.channel_id);
                config.channel_name = item.value("channel_name", config.channel_name);
                config.topic = item.value("topic", config.topic);
                configs.push_back(std::move(config));
            }
            return !configs.empty();
        }

        RuntimeConfig config;
        if (!LoadConfig(configJson, config)) {
            return false;
        }
        configs.push_back(std::move(config));
        return true;
    } catch (...) {
        return false;
    }
#endif
}

void TcpProtocolService::StartAcceptLoop() {
#if RECPLAY_HAS_BOOST_ASIO
    if (!acceptor_ || !io_context_) {
        return;
    }

    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*io_context_);
    acceptor_->async_accept(
        *socket,
        [this, socket](const boost::system::error_code& error) {
            if (error) {
                bool shouldRetry = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    shouldRetry = capturing_ && static_cast<bool>(acceptor_);
                }
                if (shouldRetry) {
                    StartAcceptLoop();
                }
                return;
            }

            auto buffer = std::make_shared<std::array<uint8_t, 65536>>();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!capturing_) {
                    return;
                }
                capture_sockets_.push_back(socket);
            }

            StartReadLoop(socket, buffer);
            StartAcceptLoop();
        });
#endif
}

#if RECPLAY_HAS_BOOST_ASIO
void TcpProtocolService::StartReadLoop(
    const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
    const std::shared_ptr<std::array<uint8_t, 65536>>& buffer) {
    if (!socket || !buffer) {
        return;
    }

    socket->async_read_some(
        boost::asio::buffer(*buffer),
        [this, socket, buffer](const boost::system::error_code& error, std::size_t bytesReceived) {
            PacketCallback callback;
            RuntimeConfig config;
            uint64_t captureEpochNs = 0;
            bool shouldRemoveSocket = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (error || !capturing_ || !capture_callback_) {
                    shouldRemoveSocket = static_cast<bool>(error);
                } else {
                    callback = capture_callback_;
                    config = capture_config_;
                    captureEpochNs = capture_epoch_ns_.load(std::memory_order_acquire);
                }
            }

            if (shouldRemoveSocket) {
                RemoveCaptureSocket(socket);
                return;
            }

            if (!callback) {
                return;
            }

            const auto captureTimestampNs = ComputeRelativeCaptureTimestamp(captureEpochNs);
            auto packet = std::make_shared<Packet>();
            packet->t_capture = captureTimestampNs;
            packet->t_origin = captureTimestampNs;
            packet->t_record = captureTimestampNs;
            packet->channel_id = config.channel_id;
            packet->topic = config.topic;
            packet->protocol_id = static_cast<uint16_t>(ProtocolId::kTCP);
            packet->sequence = sequence_.fetch_add(1, std::memory_order_acq_rel);
            packet->payload.assign(buffer->begin(), buffer->begin() + bytesReceived);
            callback(packet);

            StartReadLoop(socket, buffer);
        });
}

void TcpProtocolService::RemoveCaptureSocket(
    const std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
    if (!socket) {
        return;
    }

    boost::system::error_code ec;
    socket->cancel(ec);
    socket->close(ec);

    std::lock_guard<std::mutex> lock(mutex_);
    capture_sockets_.erase(
        std::remove(capture_sockets_.begin(), capture_sockets_.end(), socket),
        capture_sockets_.end());
    if (capture_sockets_.empty() && !acceptor_) {
        capturing_ = false;
        capture_callback_ = {};
        channels_.clear();
        capture_epoch_ns_.store(0, std::memory_order_release);
    }
}
#endif

void TcpProtocolService::ResetIoRuntimeIfUnused() {
#if RECPLAY_HAS_BOOST_ASIO
    std::thread threadToJoin;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (acceptor_ || !capture_sockets_.empty() || !replay_sockets_.empty()) {
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
    }
    if (threadToJoin.joinable()) {
        threadToJoin.join();
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (acceptor_ || !capture_sockets_.empty() || !replay_sockets_.empty()) {
            return;
        }
        work_guard_.reset();
        io_context_.reset();
    }
#endif
}

} // namespace recplay
