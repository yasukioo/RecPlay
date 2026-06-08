// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "SessionController.h"

#include "CoreEngine.h"
#include "IStorageService.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#define RECPLAY_HAS_SPDLOG 1
#else
#define RECPLAY_HAS_SPDLOG 0
#endif

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HAS_JSON 1
#else
#define RECPLAY_HAS_JSON 0
#endif

// <windows.h> (pulled in transitively, e.g. via spdlog) defines CreateFile as a
// macro aliasing CreateFileA/W. Since it is included after IStorageService.h, it
// would rewrite storage->CreateFile(...) into CreateFileA(...). Undo it here so
// the IStorageService::CreateFile member call resolves correctly.
#ifdef CreateFile
#undef CreateFile
#endif

namespace recplay {

namespace {

struct RecordRequest {
    std::string output_path;
    std::string codec = "zstd";
    std::vector<std::string> protocols;
    std::string protocol_config = "{}";
};

std::string TrimCopy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

size_t FindValueStart(const std::string& document, const std::string& key) {
    const auto keyPos = document.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return std::string::npos;
    }
    const auto colonPos = document.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return std::string::npos;
    }
    return colonPos + 1;
}

std::string ExtractJsonStringValue(const std::string& document, const std::string& key) {
    const auto valueStart = FindValueStart(document, key);
    if (valueStart == std::string::npos) {
        return {};
    }

    const auto firstQuote = document.find('"', valueStart);
    if (firstQuote == std::string::npos) {
        return {};
    }
    const auto secondQuote = document.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return {};
    }
    return document.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::vector<std::string> ExtractJsonStringArray(const std::string& document, const std::string& key) {
    const auto valueStart = FindValueStart(document, key);
    if (valueStart == std::string::npos) {
        return {};
    }

    const auto openBracket = document.find('[', valueStart);
    if (openBracket == std::string::npos) {
        return {};
    }
    const auto closeBracket = document.find(']', openBracket + 1);
    if (closeBracket == std::string::npos) {
        return {};
    }

    std::vector<std::string> values;
    size_t cursor = openBracket;
    while (true) {
        const auto firstQuote = document.find('"', cursor + 1);
        if (firstQuote == std::string::npos || firstQuote > closeBracket) {
            break;
        }
        const auto secondQuote = document.find('"', firstQuote + 1);
        if (secondQuote == std::string::npos || secondQuote > closeBracket) {
            break;
        }
        values.push_back(document.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        cursor = secondQuote;
    }
    return values;
}

std::string ExtractJsonObjectLiteral(const std::string& document, const std::string& key) {
    const auto valueStart = FindValueStart(document, key);
    if (valueStart == std::string::npos) {
        return {};
    }

    const auto openBrace = document.find('{', valueStart);
    if (openBrace == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = openBrace; i < document.size(); ++i) {
        if (document[i] == '{') {
            ++depth;
        } else if (document[i] == '}') {
            --depth;
            if (depth == 0) {
                return TrimCopy(document.substr(openBrace, i - openBrace + 1));
            }
        }
    }

    return {};
}

std::vector<std::string> CollectReplayProtocols(const std::vector<ChannelInfo>& channels,
                                                const std::vector<std::string>& fallbackProtocols) {
    std::vector<std::string> protocols;
    for (const auto& channel : channels) {
        if (channel.protocol.empty()) {
            continue;
        }
        if (std::find(protocols.begin(), protocols.end(), channel.protocol) == protocols.end()) {
            protocols.push_back(channel.protocol);
        }
    }

    if (protocols.empty()) {
        for (const auto& protocol : fallbackProtocols) {
            if (protocol.empty()) {
                continue;
            }
            if (std::find(protocols.begin(), protocols.end(), protocol) == protocols.end()) {
                protocols.push_back(protocol);
            }
        }
    }

    return protocols;
}

std::vector<std::string> InferReplayProtocolsFromConfig(const std::string& replayConfigJson) {
    std::vector<std::string> protocols;

#if RECPLAY_HAS_JSON
    try {
        const auto document = nlohmann::json::parse(replayConfigJson.empty() ? "{}" : replayConfigJson);
        if (document.is_object() &&
            (document.contains("address") || document.contains("interface") || document.contains("recv_buf"))) {
            protocols.push_back("UDP");
        } else if (document.is_object() &&
                   (document.contains("host") || document.contains("mode"))) {
            protocols.push_back("TCP");
        }
    } catch (...) {
        return {};
    }
#else
    const bool looks_like_udp =
        replayConfigJson.find("\"address\"") != std::string::npos ||
        replayConfigJson.find("\"interface\"") != std::string::npos ||
        replayConfigJson.find("\"recv_buf\"") != std::string::npos;
    const bool looks_like_tcp =
        replayConfigJson.find("\"host\"") != std::string::npos ||
        replayConfigJson.find("\"mode\"") != std::string::npos;
    if (looks_like_udp) {
        protocols.push_back("UDP");
    } else if (looks_like_tcp) {
        protocols.push_back("TCP");
    }
#endif

    return protocols;
}

std::string BuildJsonObjectLiteral(const std::map<std::string, std::string>& values) {
#if RECPLAY_HAS_JSON
    nlohmann::json document = nlohmann::json::object();
    for (const auto& [key, value] : values) {
        document[key] = value;
    }
    return document.dump();
#else
    std::ostringstream stream;
    stream << "{";
    bool first = true;
    for (const auto& [key, value] : values) {
        if (!first) {
            stream << ",";
        }
        first = false;
        stream << "\"" << key << "\":\"" << value << "\"";
    }
    stream << "}";
    return stream.str();
#endif
}

bool ReplayTargetHasRunnableConfig(const ISessionService::ReplayTarget& target);

std::vector<std::string> BuildReplayProtocolsFromTargets(
    const std::vector<ISessionService::ReplayTarget>& targets) {
    std::vector<std::string> protocols;
    for (const auto& target : targets) {
        if (!ReplayTargetHasRunnableConfig(target) || target.protocol.empty()) {
            continue;
        }
        if (std::find(protocols.begin(), protocols.end(), target.protocol) == protocols.end()) {
            protocols.push_back(target.protocol);
        }
    }
    return protocols;
}

std::string BuildReplayConfigForProtocol(
    const std::vector<ISessionService::ReplayTarget>& targets,
    const std::string& protocol,
    const std::string& fallbackConfig) {
    std::vector<std::map<std::string, std::string>> configs;
    for (const auto& target : targets) {
        if (!target.enabled || target.protocol != protocol) {
            continue;
        }
        configs.push_back(target.config);
    }

    if (configs.empty()) {
        return fallbackConfig;
    }

#if RECPLAY_HAS_JSON
    if (configs.size() == 1) {
        return BuildJsonObjectLiteral(configs.front());
    }
    nlohmann::json document = nlohmann::json::array();
    for (const auto& config : configs) {
        nlohmann::json item = nlohmann::json::object();
        for (const auto& [key, value] : config) {
            item[key] = value;
        }
        document.push_back(std::move(item));
    }
    return document.dump();
#else
    return BuildJsonObjectLiteral(configs.front());
#endif
}

bool ReplayTargetHasRunnableConfig(const ISessionService::ReplayTarget& target) {
    if (!target.enabled) {
        return false;
    }

    if (target.protocol == "UDP") {
        return target.config.find("address") != target.config.end() &&
               target.config.find("port") != target.config.end();
    }
    if (target.protocol == "TCP") {
        return target.config.find("host") != target.config.end() &&
               target.config.find("port") != target.config.end();
    }

    return !target.config.empty();
}

bool ReplayConfigIdentifiesProtocol(const std::string& protocol,
                                    const std::string& replayConfigJson) {
    const auto protocols = InferReplayProtocolsFromConfig(replayConfigJson);
    return std::find(protocols.begin(), protocols.end(), protocol) != protocols.end();
}

bool HasRunnableReplayTargetForProtocol(const std::vector<ISessionService::ReplayTarget>& targets,
                                        const std::string& protocol) {
    return std::any_of(
        targets.begin(),
        targets.end(),
        [&protocol](const ISessionService::ReplayTarget& target) {
            return target.protocol == protocol && ReplayTargetHasRunnableConfig(target);
        });
}

#if RECPLAY_HAS_JSON
std::string ToJsonString(const nlohmann::json& value) {
    return value.dump();
}
#endif

RecordRequest ParseRecordRequest(const std::string& config_json,
                                 const std::vector<std::string>& available_protocols) {
    RecordRequest request;
#if RECPLAY_HAS_JSON
    const auto document = nlohmann::json::parse(config_json.empty() ? "{}" : config_json);
    request.output_path = document.value("output_path", std::string{});
    request.codec = document.value("codec", std::string("zstd"));

    if (document.contains("protocols") && document.at("protocols").is_array()) {
        request.protocols = document.at("protocols").get<std::vector<std::string>>();
    }
    if (request.protocols.empty()) {
        request.protocols = available_protocols;
    }

    if (document.contains("protocol_config")) {
        request.protocol_config = ToJsonString(document.at("protocol_config"));
    } else if (document.contains("protocol")) {
        request.protocol_config = ToJsonString(document.at("protocol"));
    }
#else
    request.output_path = ExtractJsonStringValue(config_json, "output_path");
    request.codec = ExtractJsonStringValue(config_json, "codec");
    if (request.codec.empty()) {
        request.codec = "zstd";
    }
    request.protocols = ExtractJsonStringArray(config_json, "protocols");
    request.protocol_config = ExtractJsonObjectLiteral(config_json, "protocol_config");
    if (request.protocol_config.empty()) {
        request.protocol_config = ExtractJsonObjectLiteral(config_json, "protocol");
    }
    if (request.protocol_config.empty()) {
        request.protocol_config = "{}";
    }
    if (request.protocols.empty()) {
        request.protocols = available_protocols;
    }
#endif
    return request;
}

} // namespace

SessionController::SessionController(CoreEngine* engine) : engine_(engine) {
    machine_.OnTransition([this](SessionState from, SessionState to) {
        PublishState(from, to);
    });
}

SessionState SessionController::GetState() const {
    return machine_.GetState();
}

void SessionController::OnStateChanged(StateCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_callbacks_.push_back(std::move(cb));
}

bool SessionController::StartRecording(const std::string& configJson) {
    if (engine_ == nullptr) {
        return false;
    }

    // Allow restarting a new recording after a previous session ended (Stopped→Idle).
    // Without this, the state machine would reject Stopped→Recording even though
    // all resources have already been released by StopRecording/Stop.
    if (machine_.GetState() == SessionState::Stopped) {
        machine_.Transition(SessionState::Idle);
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr) {
        return false;
    }

    const auto available_protocols = engine_->GetAttachedProtocolNames();
    const auto request = ParseRecordRequest(configJson, available_protocols);
    if (request.output_path.empty() || request.protocols.empty()) {
        return false;
    }

    std::vector<ChannelInfo> channels;
    for (const auto& protocol_name : request.protocols) {
        auto protocol_channels = engine_->GetChannelsForProtocol(protocol_name);
        channels.insert(channels.end(), protocol_channels.begin(), protocol_channels.end());
    }

    if (!storage->CreateFile(request.output_path, channels, request.codec)) {
        return false;
    }

    std::vector<std::string> started_protocols;
    for (const auto& protocol_name : request.protocols) {
        if (!engine_->StartProtocolCapture(protocol_name, request.protocol_config)) {
            for (const auto& started_name : started_protocols) {
                engine_->StopProtocolCapture(started_name);
            }
            storage->FinalizeFile();
            return false;
        }
        started_protocols.push_back(protocol_name);
    }

    if (!machine_.Transition(SessionState::Recording)) {
        for (const auto& started_name : started_protocols) {
            engine_->StopProtocolCapture(started_name);
        }
        storage->FinalizeFile();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_record_protocols_ = std::move(started_protocols);
        current_record_path_ = request.output_path;
        current_playback_file_.clear();
        current_playback_packet_ = {};
        current_position_ns_ = 0;
        duration_ns_ = 0;
    }
    engine_->Timeline().SetOrigin(0);
    engine_->Timeline().Start();
    return true;
}

void SessionController::PauseRecording() {
    if (machine_.GetState() != SessionState::Recording) {
        return;
    }
    if (!machine_.Transition(SessionState::RecordingPaused)) {
        return;
    }
    if (engine_ != nullptr) {
        engine_->Timeline().Pause();
    }
}

void SessionController::ResumeRecording() {
    if (machine_.GetState() != SessionState::RecordingPaused) {
        return;
    }
    if (!machine_.Transition(SessionState::Recording)) {
        return;
    }
    if (engine_ != nullptr) {
        engine_->Timeline().Resume();
    }
}

void SessionController::StopRecording() {
    if (engine_ != nullptr) {
        StopRecordingProtocols();
        if (auto* storage = engine_->Storage(); storage != nullptr && storage->IsWriting()) {
            storage->FinalizeFile();
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_playback_packet_ = {};
    }
    machine_.Transition(SessionState::Stopped);
}

bool SessionController::OpenForPlayback(const std::string& filePath,
                                       const std::string& replayConfigJson) {
    if (engine_ == nullptr) {
#if RECPLAY_HAS_SPDLOG
        spdlog::error("playback open failed: core engine unavailable");
#endif
        return false;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->OpenFile(filePath)) {
#if RECPLAY_HAS_SPDLOG
        spdlog::error("playback open failed: storage could not open file {}", filePath);
#endif
        return false;
    }

    const std::string replay_config = replayConfigJson.empty() ? "{}" : replayConfigJson;
    std::vector<ReplayTarget> replay_targets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        replay_targets = replay_targets_;
    }

    auto protocol_names = BuildReplayProtocolsFromTargets(replay_targets);
    if (protocol_names.empty()) {
        protocol_names = CollectReplayProtocols(storage->GetChannels(), {});
        if (protocol_names.empty()) {
            protocol_names = InferReplayProtocolsFromConfig(replay_config);
        }
        if (protocol_names.empty()) {
            protocol_names = engine_->GetAttachedProtocolNames();
        }
    }
#if RECPLAY_HAS_SPDLOG
    spdlog::info("playback open: file={} inferred_protocols={}", filePath, protocol_names.size());
#endif
    std::vector<std::string> replay_protocols;
    for (const auto& protocol_name : protocol_names) {
        if (protocol_name == "TCP" &&
            !HasRunnableReplayTargetForProtocol(replay_targets, protocol_name) &&
            !ReplayConfigIdentifiesProtocol(protocol_name, replay_config)) {
            continue;
        }
        const std::string protocol_config =
            BuildReplayConfigForProtocol(replay_targets, protocol_name, replay_config);
        if (!engine_->StartProtocolReplay(protocol_name, protocol_config)) {
#if RECPLAY_HAS_SPDLOG
            spdlog::error("playback open failed: protocol replay start failed for {}", protocol_name);
#endif
            for (const auto& started_name : replay_protocols) {
                engine_->StopProtocolReplay(started_name);
            }
            storage->CloseFile();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                active_replay_protocols_.clear();
                RefreshReplayTargetStatusesLocked(&protocol_name);
            }
            return false;
        }
        replay_protocols.push_back(protocol_name);
    }

    const auto header = storage->GetHeader();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_playback_file_ = filePath;
        active_replay_protocols_ = std::move(replay_protocols);
        current_playback_packet_ = {};
        duration_ns_ = header.duration_ns;
        current_position_ns_ = 0;
        RefreshReplayTargetStatusesLocked();
    }

    engine_->Timeline().SetOrigin(0);
    engine_->Scheduler().Clear();
    if (!FillPlaybackQueue()) {
#if RECPLAY_HAS_SPDLOG
        spdlog::error("playback open failed: initial playback queue fill returned false");
#endif
        StopReplayProtocols();
        storage->CloseFile();
        return false;
    }

    // Land in the "staged / ready to play" state. After a record→stop flow the
    // session is already Stopped, and Stopped→Stopped is not a valid transition,
    // so treat "already Stopped" as success rather than failing the open.
    if (machine_.GetState() == SessionState::Stopped) {
        return true;
    }
    return machine_.Transition(SessionState::Stopped);
}

void SessionController::Play(double speed) {
    if (engine_ == nullptr) {
        return;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return;
    }

    current_speed_.store(speed, std::memory_order_release);
    engine_->Timeline().SetSpeed(speed);
    if (engine_->Timeline().IsPaused()) {
        engine_->Timeline().Resume();
    } else {
        engine_->Timeline().Start();
    }
    engine_->Scheduler().Start([this](PacketPtr pkt) { DispatchPlaybackPacket(std::move(pkt)); });
    machine_.Transition(SessionState::Playing);
}

void SessionController::Pause() {
    if (machine_.GetState() != SessionState::Playing) {
        return;
    }
    if (engine_) {
        engine_->Timeline().Pause();
        engine_->Scheduler().Stop();
    }
    machine_.Transition(SessionState::PlaybackPaused);
}

void SessionController::SeekTo(uint64_t timestamp_ns) {
    if (engine_ == nullptr) {
        return;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return;
    }

    const auto previous_state = machine_.GetState();
    if (!machine_.Transition(SessionState::Seeking)) {
        return;
    }

    engine_->Scheduler().Stop();
    engine_->Scheduler().Clear();
    if (!storage->SeekTo(timestamp_ns)) {
        if (previous_state == SessionState::PlaybackPaused) {
            machine_.Transition(SessionState::PlaybackPaused);
        } else {
            machine_.Transition(SessionState::Playing);
        }
        return;
    }

    engine_->Timeline().SeekTo(timestamp_ns);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_position_ns_ = timestamp_ns;
    }

    FillPlaybackQueue();

    if (previous_state == SessionState::PlaybackPaused) {
        machine_.Transition(SessionState::PlaybackPaused);
    } else {
        Play(current_speed_.load(std::memory_order_acquire));
    }
}

void SessionController::SetSpeed(double speed) {
    current_speed_.store(speed, std::memory_order_release);
    if (engine_) {
        engine_->Timeline().SetSpeed(speed);
    }
}

void SessionController::SetLoopRange(uint64_t startNs, uint64_t endNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    loop_start_ns_ = startNs;
    loop_end_ns_ = endNs;
}

void SessionController::Stop() {
    const auto state = machine_.GetState();
    const bool has_active_playback =
        engine_ != nullptr &&
        ((engine_->Storage() != nullptr && engine_->Storage()->IsReading()) ||
         !active_replay_protocols_.empty());
    if (state != SessionState::Playing &&
        state != SessionState::PlaybackPaused &&
        state != SessionState::Seeking &&
        !(state == SessionState::Stopped && has_active_playback)) {
        return;
    }
    if (engine_ != nullptr) {
        engine_->Scheduler().Stop();
        engine_->Scheduler().Clear();
        StopReplayProtocols();
        if (auto* storage = engine_->Storage(); storage != nullptr && storage->IsReading()) {
            storage->CloseFile();
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_playback_packet_ = {};
    }
    if (state == SessionState::Seeking) {
        machine_.Transition(SessionState::PlaybackPaused);
    }
    machine_.Transition(SessionState::Stopped);
}

void SessionController::Reset() {
    // Transition from Stopped (or Idle) back to Idle so a new recording can start
    // without restarting the process.  No-op if already Idle or in an active state.
    const auto state = machine_.GetState();
    if (state == SessionState::Stopped) {
        machine_.Transition(SessionState::Idle);
    }
}

uint64_t SessionController::GetDuration() const {
    // During an active recording the file hasn't been finalised yet, so
    // duration_ns_ is still 0.  Return the live timeline elapsed time instead
    // so the HTTP /api/session/state and WebSocket stats reflect real elapsed time.
    const auto state = machine_.GetState();
    if (engine_ != nullptr &&
        (state == SessionState::Recording || state == SessionState::RecordingPaused)) {
        return engine_->Timeline().Now();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return duration_ns_;
}

uint64_t SessionController::GetCurrentPosition() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_position_ns_;
}

double SessionController::GetCurrentSpeed() const {
    return current_speed_.load(std::memory_order_acquire);
}

ISessionService::PlaybackPacketSnapshot SessionController::GetCurrentPlaybackPacket() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_playback_packet_;
}

std::vector<ISessionService::ReplayTarget> SessionController::GetReplayTargets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return replay_targets_;
}

void SessionController::SetReplayTargets(const std::vector<ReplayTarget>& targets) {
    std::lock_guard<std::mutex> lock(mutex_);
    replay_targets_ = targets;
    RefreshReplayTargetStatusesLocked();
}

void SessionController::StopRecordingProtocols() {
    if (engine_ == nullptr) {
        return;
    }

    std::vector<std::string> protocols;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        protocols = active_record_protocols_;
        active_record_protocols_.clear();
    }

    for (const auto& protocol_name : protocols) {
        engine_->StopProtocolCapture(protocol_name);
    }
}

void SessionController::StopReplayProtocols() {
    if (engine_ == nullptr) {
        return;
    }

    std::vector<std::string> protocols;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        protocols = active_replay_protocols_;
        active_replay_protocols_.clear();
        RefreshReplayTargetStatusesLocked();
    }

    for (const auto& protocol_name : protocols) {
        engine_->StopProtocolReplay(protocol_name);
    }
}

bool SessionController::FillPlaybackQueue() {
    if (engine_ == nullptr) {
        return false;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return false;
    }

    bool queued_any = false;
    size_t queued_count = 0;
    while (storage->HasMore() && queued_count < kPlaybackPrefetchPackets) {
        auto pkt = storage->ReadNext();
        if (!pkt) {
            break;
        }
        engine_->Scheduler().Enqueue(pkt);
        queued_any = true;
        ++queued_count;
    }
    return queued_any || !storage->HasMore();
}

void SessionController::DispatchPlaybackPacket(PacketPtr pkt) {
    if (engine_ == nullptr || !pkt) {
        return;
    }

    auto* storage = engine_->Storage();
    if (storage == nullptr || !storage->IsReading()) {
        return;
    }

    uint64_t loop_start = 0;
    uint64_t loop_end = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_position_ns_ = pkt->t_capture;
        current_playback_packet_.available = true;
        current_playback_packet_.packet = *pkt;
        current_playback_packet_.replay_timestamp_ns = engine_->Timeline().Now();
        current_playback_packet_.writer = current_playback_file_;
        loop_start = loop_start_ns_;
        loop_end = loop_end_ns_;
    }

    engine_->DispatchPacketToReplayingProtocols(pkt);

    if (loop_end > loop_start && pkt->t_capture >= loop_end) {
        engine_->Scheduler().Clear();
        if (storage->SeekTo(loop_start)) {
            engine_->Timeline().SeekTo(loop_start);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current_position_ns_ = loop_start;
            }
            FillPlaybackQueue();
        }
        return;
    }

    FillPlaybackQueue();
}

void SessionController::PublishState(SessionState from, SessionState next) {
    std::vector<StateCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks = state_callbacks_;
    }
    for (const auto& callback : callbacks) {
        if (callback) {
            callback(from, next);
        }
    }
}

void SessionController::RefreshReplayTargetStatusesLocked(const std::string* failingProtocol) {
    for (auto& target : replay_targets_) {
        if (!target.enabled) {
            target.status = "idle";
            continue;
        }
        if (!ReplayTargetHasRunnableConfig(target)) {
            target.status = "error";
            continue;
        }
        if (failingProtocol != nullptr && target.protocol == *failingProtocol) {
            target.status = "error";
            continue;
        }

        const bool protocolActive =
            std::find(active_replay_protocols_.begin(),
                      active_replay_protocols_.end(),
                      target.protocol) != active_replay_protocols_.end();
        target.status = protocolActive ? "active" : "idle";
    }
}

} // namespace recplay
