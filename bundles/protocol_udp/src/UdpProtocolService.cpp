// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "UdpProtocolService.h"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

#include <chrono>
#include <utility>

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

        auto socket = std::make_unique<boost::asio::ip::udp::socket>(*activeIoContext);

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

        {
            std::lock_guard<std::mutex> lock(mutex_);
            capture_config_ = config;
            capture_callback_ = std::move(cb);
            if (createdIoContext) {
                io_context_ = std::move(createdIoContext);
            }
            if (createdGuard) {
                work_guard_ = std::move(createdGuard);
            }
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
            capture_epoch_ns_.store(GetSteadyClockNanoseconds(), std::memory_order_release);
        }

        StartReceiveLoop();
        if (startIoThread) {
            io_thread_ = std::thread([activeIoContext] {
                if (activeIoContext) {
                    activeIoContext->run();
                }
            });
        }
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
        capturing_ = false;
        capture_callback_ = {};
        channels_.clear();
        if (capture_socket_) {
            boost::system::error_code ec;
            capture_socket_->cancel(ec);
            capture_socket_->close(ec);
            capture_socket_.reset();
        }
    }
    ResetIoRuntimeIfUnused();
    std::lock_guard<std::mutex> lock(mutex_);
    capture_epoch_ns_.store(0, std::memory_order_release);
#endif
}

bool UdpProtocolService::IsCapturing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capturing_;
}

bool UdpProtocolService::StartReplay(const std::string& configJson) {
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

        std::vector<std::unique_ptr<boost::asio::ip::udp::socket>> sockets;
        sockets.reserve(configs.size());
        for (const auto& config : configs) {
            auto socket = std::make_unique<boost::asio::ip::udp::socket>(*activeIoContext);
            socket->open(boost::asio::ip::udp::v4());
            socket->bind(boost::asio::ip::udp::endpoint(
                boost::asio::ip::make_address(config.bind_interface),
                0));
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
            replaying_ = false;
            replay_sockets_.clear();
            replay_configs_.clear();
        }
        ResetIoRuntimeIfUnused();
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
    if (!replaying_ || replay_sockets_.empty() || replay_configs_.empty()) {
        return false;
    }

    bool sentAny = false;
    for (std::size_t index = 0; index < replay_sockets_.size() && index < replay_configs_.size(); ++index) {
        const auto& socket = replay_sockets_[index];
        const auto& config = replay_configs_[index];
        if (!socket) {
            continue;
        }

        const auto endpoint = boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address(config.address),
            config.port);
        boost::system::error_code ec;
        socket->send_to(boost::asio::buffer(pkt->payload), endpoint, 0, ec);
        if (ec) {
            return false;
        }
        sentAny = true;
    }
    return sentAny;
#endif
}

void UdpProtocolService::StopReplay() {
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
    try {
        const auto json = nlohmann::json::parse(configJson.empty() ? "{}" : configJson);
        config.address = json.value("address", config.address);
        config.bind_interface = json.value("interface", config.bind_interface);
        config.port = static_cast<unsigned short>(json.value("port", static_cast<int>(config.port)));
        config.recv_buf = json.value("recv_buf", config.recv_buf);
        config.channel_id = json.value("channel_id", config.channel_id);
        config.channel_name = json.value("channel_name", config.channel_name);
        config.topic = json.value("topic", config.topic);
        return true;
    } catch (...) {
        return false;
    }
#endif
}

bool UdpProtocolService::LoadReplayConfigs(const std::string& configJson, std::vector<RuntimeConfig>& configs) const {
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
                config.address = item.value("address", config.address);
                config.bind_interface = item.value("interface", item.value("bind_interface", config.bind_interface));
                config.port = static_cast<unsigned short>(item.value("port", static_cast<int>(config.port)));
                config.recv_buf = item.value("recv_buf", config.recv_buf);
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
            uint64_t captureEpochNs = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (error || !capturing_ || !capture_callback_) {
                    return;
                }
                callback = capture_callback_;
                config = capture_config_;
                captureEpochNs = capture_epoch_ns_.load(std::memory_order_acquire);
            }

            const auto captureTimestampNs = ComputeRelativeCaptureTimestamp(captureEpochNs);
            auto packet = std::make_shared<Packet>();
            packet->t_capture = captureTimestampNs;
            packet->t_origin = captureTimestampNs;
            packet->t_record = captureTimestampNs;
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
        if (capture_socket_ || !replay_sockets_.empty()) {
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
        if (capture_socket_ || !replay_sockets_.empty()) {
            return;
        }
        work_guard_.reset();
        io_context_.reset();
    }
#endif
}

} // namespace recplay
