// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "IRuntimeCatalog.h"
#include "ISessionService.h"
#include "IStorageService.h"
#include "IStatsService.h"
#include "WebSocketServer.h"
#include "WebRootLocator.h"

#if __has_include(<nlohmann/json.hpp>)
#define RECPLAY_TEST_HAS_JSON 1
#else
#define RECPLAY_TEST_HAS_JSON 0
#endif

#include <cstddef>
#include <limits>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using recplay::HttpServer;
using recplay::IStatsService;
using recplay::IRuntimeCatalog;
using recplay::ISessionService;
using recplay::IStorageService;
using recplay::PluginConfigFieldInfo;
using recplay::PluginRuntimeInfo;
using recplay::RecordingFileInfo;
using recplay::RuntimeChannelInfo;
using recplay::RuntimeTopicMapping;
using recplay::SessionState;
using recplay::StateCallback;
using recplay::StatsSnapshot;
using recplay::WebSocketServer;
using recplay::ResolveWebRoot;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeSessionService final : public ISessionService {
public:
    SessionState GetState() const override { return state; }
    void OnStateChanged(StateCallback cb) override { callback = std::move(cb); }
    bool StartRecording(const std::string& config) override {
        start_record_called = true;
        last_record_config = config;
        return start_record_result;
    }
    void PauseRecording() override { pause_record_called = true; }
    void ResumeRecording() override { resume_record_called = true; }
    void StopRecording() override { stop_record_called = true; }
    bool OpenForPlayback(const std::string& path, const std::string& = "{}") override {
        open_playback_called = true;
        last_open_path = path;
        return open_playback_result;
    }
    void Play(double value) override {
        play_called = true;
        last_play_speed = value;
    }
    void Pause() override { pause_called = true; }
    void SeekTo(uint64_t value) override {
        seek_called = true;
        last_seek_ns = value;
    }
    void SetSpeed(double value) override {
        set_speed_called = true;
        speed = value;
    }
    void SetLoopRange(uint64_t start, uint64_t end) override {
        loop_called = true;
        last_loop_start_ns = start;
        last_loop_end_ns = end;
    }
    void Stop() override { stop_called = true; }
    void Reset() override { reset_called = true; }
    uint64_t GetDuration() const override { return duration; }
    uint64_t GetCurrentPosition() const override { return position; }
    double GetCurrentSpeed() const override { return speed; }
    PlaybackPacketSnapshot GetCurrentPlaybackPacket() const override { return playback_packet; }
    std::vector<ReplayTarget> GetReplayTargets() const override { return replay_targets; }
    void SetReplayTargets(const std::vector<ReplayTarget>& targets) override { replay_targets = targets; }

    SessionState state = SessionState::Playing;
    uint64_t duration = 1000;
    uint64_t position = 250;
    double speed = 2.0;
    StateCallback callback;
    double last_play_speed = 0.0;
    uint64_t last_seek_ns = 0;
    uint64_t last_loop_start_ns = 0;
    uint64_t last_loop_end_ns = 0;
    std::string last_record_config;
    std::string last_open_path;
    bool play_called = false;
    bool pause_called = false;
    bool seek_called = false;
    bool loop_called = false;
    bool stop_called = false;
    bool reset_called = false;
    bool set_speed_called = false;
    bool start_record_called = false;
    bool pause_record_called = false;
    bool resume_record_called = false;
    bool stop_record_called = false;
    bool open_playback_called = false;
    bool start_record_result = true;
    bool open_playback_result = true;
    PlaybackPacketSnapshot playback_packet;
    std::vector<ReplayTarget> replay_targets;
};

class FakeStatsService final : public IStatsService {
public:
    StatsSnapshot GetSnapshot() const override { return snapshot; }
    void OnUpdate(std::function<void(const StatsSnapshot&)> cb) override { callback = std::move(cb); }
    void RecordPacket(const std::string&, uint32_t, size_t) override {}
    void RecordDrop(const std::string&, uint64_t) override {}
    void RecordWriteLatency(double) override {}
    void UpdateRingBufferState(uint32_t, uint32_t) override {}
    void UpdateDiskQueue(uint64_t) override {}

    StatsSnapshot snapshot;
    std::function<void(const StatsSnapshot&)> callback;
};

class FakeRuntimeCatalog final : public IRuntimeCatalog {
public:
    std::vector<PluginRuntimeInfo> ListPlugins() const override { return plugins; }

    std::optional<PluginRuntimeInfo> GetPlugin(const std::string& id) const override {
        for (const auto& plugin : plugins) {
            if (plugin.id == id) {
                return plugin;
            }
        }
        return std::nullopt;
    }

    bool SavePluginConfig(const std::string& id,
                          const std::vector<PluginConfigFieldInfo>& fields) override {
        saved_plugin_id = id;
        saved_fields = fields;
        return save_plugin_config_result;
    }

    bool StartPlugin(const std::string& id) override {
        started_plugin_id = id;
        return start_plugin_result;
    }

    bool StopPlugin(const std::string& id) override {
        stopped_plugin_id = id;
        return stop_plugin_result;
    }

    std::vector<RuntimeChannelInfo> ListChannels() const override { return channels; }

    std::vector<RuntimeTopicMapping> GetTopicMappings() const override { return mappings; }

    bool SetTopicMappings(const std::vector<RuntimeTopicMapping>& nextMappings) override {
        set_mappings_called = true;
        mappings = nextMappings;
        return set_topic_mappings_result;
    }

    std::vector<PluginRuntimeInfo> plugins;
    std::vector<RuntimeChannelInfo> channels;
    std::vector<RuntimeTopicMapping> mappings;
    std::string saved_plugin_id;
    std::vector<PluginConfigFieldInfo> saved_fields;
    std::string started_plugin_id;
    std::string stopped_plugin_id;
    bool save_plugin_config_result = true;
    bool start_plugin_result = true;
    bool stop_plugin_result = true;
    bool set_topic_mappings_result = true;
    bool set_mappings_called = false;
};

class FakeStorageService final : public IStorageService {
public:
    bool CreateFile(const std::string&,
                    const std::vector<recplay::ChannelInfo>&,
                    const std::string&) override {
        return false;
    }
    bool WritePacket(recplay::PacketPtr) override { return false; }
    bool FinalizeFile() override { return false; }
    bool OpenFile(const std::string&) override { return false; }
    void CloseFile() override {}
    recplay::RpcapHeader GetHeader() const override { return {}; }
    std::vector<recplay::ChannelInfo> GetChannels() const override { return {}; }
    bool SeekTo(uint64_t) override { return false; }
    recplay::PacketPtr ReadNext() override { return nullptr; }
    bool HasMore() const override { return false; }
    bool IsWriting() const override { return false; }
    bool IsReading() const override { return false; }
    std::vector<uint64_t> GetKeyframeTimestamps() const override { return {}; }
    std::optional<RecordingFileInfo> ProbeFile(const std::string& path) const override {
        last_probe_path = path;
        return file_info;
    }
    std::vector<uint64_t> GetDensity(uint32_t) const override { return {}; }

    mutable std::string last_probe_path;
    std::optional<RecordingFileInfo> file_info;
};

void TestSessionStateResponse() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest("GET", "/api/session/state", "");
    Expect(response.status_code == 200, "session state endpoint should return 200");
    Expect(response.content_type == "application/json", "session state should be json");
    Expect(response.body.find("\"state\":\"Playing\"") != std::string::npos, "body should include state");
    Expect(response.body.find("\"duration_ns\":1000") != std::string::npos, "body should include duration");
    Expect(response.body.find("\"position_ns\":250") != std::string::npos, "body should include position");
    Expect(response.body.find("\"speed\":2") != std::string::npos, "body should include speed");
}

void TestStatsSnapshotResponse() {
    FakeSessionService session;
    FakeStatsService stats;
    stats.snapshot.total_packets = 42;
    stats.snapshot.total_drops = 3;
    stats.snapshot.total_throughput_mbps = 12.5;
    stats.snapshot.write_latency_p99_ms = 4.75;
    stats.snapshot.ringbuf_used = 7;
    stats.snapshot.ringbuf_capacity = 64;
    stats.snapshot.disk_queue_bytes = 8192;
    stats.snapshot.cpu_usage_percent = 37.5;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 200, "stats endpoint should return 200");
    Expect(response.body.find("\"total_packets\":42") != std::string::npos, "body should include packet count");
    Expect(response.body.find("\"total_drops\":3") != std::string::npos, "body should include drop count");
    Expect(response.body.find("\"total_throughput_mbps\":12.5") != std::string::npos, "body should include throughput");
    Expect(response.body.find("\"write_latency_p99_ms\":4.75") != std::string::npos, "body should include p99");
    Expect(response.body.find("\"ringbuf_used\":7") != std::string::npos, "body should include ring buffer");
    Expect(response.body.find("\"disk_queue_bytes\":8192") != std::string::npos, "body should include disk queue");
    Expect(response.body.find("\"cpu_usage_percent\":37.5") != std::string::npos,
           "body should include actual cpu usage percent");
}

void TestStatsSnapshotResponseSanitizesNonFiniteNumbers() {
    FakeSessionService session;
    FakeStatsService stats;
    stats.snapshot.total_throughput_mbps = std::numeric_limits<double>::infinity();
    stats.snapshot.drop_rate = std::numeric_limits<double>::quiet_NaN();
    stats.snapshot.write_latency_p99_ms = -std::numeric_limits<double>::infinity();
    stats.snapshot.cpu_usage_percent = std::numeric_limits<double>::quiet_NaN();
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 200, "stats endpoint should still return 200 for non-finite values");
    Expect(response.body.find("\"total_throughput_mbps\":0") != std::string::npos,
           "throughput should be sanitized to a valid JSON number");
    Expect(response.body.find("\"drop_rate\":0") != std::string::npos,
           "drop rate should be sanitized to a valid JSON number");
    Expect(response.body.find("\"write_latency_p99_ms\":0") != std::string::npos,
           "latency should be sanitized to a valid JSON number");
    Expect(response.body.find("\"cpu_usage_percent\":null") != std::string::npos,
           "cpu usage should become null when the source value is non-finite");
}

void TestRuntimeRoutes() {
    FakeSessionService session;
    FakeStatsService stats;
    FakeRuntimeCatalog runtime;
    WebSocketServer websocket(&session, &stats);
    runtime.plugins = {
        PluginRuntimeInfo{
            "UDP",
            "UDP Protocol",
            "1.0.0",
            "inactive",
            "Source",
            "UDP",
            "UDP Source",
            "bundles/protocol_udp.dll",
            {
                PluginConfigFieldInfo{"address", "Address", "239.1.1.1"},
                PluginConfigFieldInfo{"port", "Port", "5000"},
            },
        },
    };
    runtime.channels = {
        RuntimeChannelInfo{"UDP:7", "Radar In", "/in/radar", "input", "UDP", "UDP", 7},
        RuntimeChannelInfo{"TCP:8", "Radar Out", "/out/radar", "output", "TCP", "TCP", 8},
    };
    runtime.mappings = {
        RuntimeTopicMapping{"/in/radar", "/out/radar"},
    };

    HttpServer server(&session, &stats);
    server.SetRuntimeCatalog(&runtime);
    server.SetWebSocketServer(&websocket);
    websocket.Start();

    auto response = server.HandleRequest("GET", "/api/plugins", "");

#if RECPLAY_TEST_HAS_JSON
    Expect(response.status_code == 200, "plugins route should return 200");
    Expect(response.body.find("\"id\":\"UDP\"") != std::string::npos,
           "plugins route should include plugin identifier");
    Expect(response.body.find("\"bundle_path\":\"bundles/protocol_udp.dll\"") != std::string::npos,
           "plugins route should include bundle path");
    Expect(response.body.find("\"kind\":\"Source\"") != std::string::npos,
           "plugins route should include plugin kind");
    Expect(response.body.find("\"protocol\":\"UDP\"") != std::string::npos,
           "plugins route should include plugin protocol");
    Expect(response.body.find("\"desc\":\"UDP Source\"") != std::string::npos,
           "plugins route should include plugin description");

    response = server.HandleRequest("GET", "/api/plugins/UDP", "");
    Expect(response.status_code == 200, "plugin detail route should return 200");
    Expect(response.body.find("\"config_fields\"") != std::string::npos,
           "plugin detail route should include config fields");

    response = server.HandleRequest(
        "POST",
        "/api/plugins/UDP/config",
        R"({"config_fields":[{"key":"address","label":"Address","value":"239.9.9.9"},{"key":"port","label":"Port","value":"5500"}]})");
    Expect(response.status_code == 200, "plugin config route should return 200");
    Expect(runtime.saved_plugin_id == "UDP", "plugin config route should save the targeted plugin id");
    Expect(runtime.saved_fields.size() == 2, "plugin config route should persist runtime fields");
    Expect(runtime.saved_fields[0].value == "239.9.9.9",
           "plugin config route should forward updated values");

    response = server.HandleRequest("POST", "/api/plugins/UDP/start", "");
    Expect(response.status_code == 200, "plugin start route should return 200");
    Expect(runtime.started_plugin_id == "UDP", "plugin start route should target the selected plugin");

    response = server.HandleRequest("POST", "/api/plugins/UDP/stop", "");
    Expect(response.status_code == 200, "plugin stop route should return 200");
    Expect(runtime.stopped_plugin_id == "UDP", "plugin stop route should target the selected plugin");

    response = server.HandleRequest("GET", "/api/channels", "");
    Expect(response.status_code == 200, "channels route should return 200");
    Expect(response.body.find("\"name\":\"Radar Out\"") != std::string::npos,
           "channels route should include channel names");
    Expect(response.body.find("\"direction\":\"output\"") != std::string::npos,
           "channels route should include channel directions");

    response = server.HandleRequest("GET", "/api/playback/packet", "");
    Expect(response.status_code == 200, "playback packet route should return 200");
    Expect(response.body == "null", "playback packet route should currently degrade to null");

    session.playback_packet.available = true;
    session.playback_packet.packet.channel_id = 7;
    session.playback_packet.packet.sequence = 42;
    session.playback_packet.packet.protocol_id = static_cast<uint16_t>(recplay::ProtocolId::kUDP);
    session.playback_packet.packet.t_record = 1234;
    session.playback_packet.packet.topic = "/in/radar";
    session.playback_packet.packet.payload = {0x01, 0x02, 0xa0};
    session.playback_packet.replay_timestamp_ns = 5678;
    session.playback_packet.writer = "capture.rpcap";

    response = server.HandleRequest("GET", "/api/playback/packet", "");
    Expect(response.body.find("\"channel\":7") != std::string::npos,
           "playback packet route should include channel id");
    Expect(response.body.find("\"plugin\":\"UDP\"") != std::string::npos,
           "playback packet route should expose protocol as plugin label");
    Expect(response.body.find("\"seq\":42") != std::string::npos,
           "playback packet route should include packet sequence");
    Expect(response.body.find("\"t_record\":1234") != std::string::npos,
           "playback packet route should include record timestamp");
    Expect(response.body.find("\"t_replay\":5678") != std::string::npos,
           "playback packet route should include replay timestamp");
    Expect(response.body.find("\"writer\":\"capture.rpcap\"") != std::string::npos,
           "playback packet route should include writer label");
    Expect(response.body.find("\"hex\":\"01 02 a0\"") != std::string::npos,
           "playback packet route should include hex preview");

    response = server.HandleRequest("GET", "/api/playback/targets", "");
    Expect(response.status_code == 200, "playback targets route should return 200");
    Expect(response.body.find("\"targets\":[]") != std::string::npos,
           "playback targets route should start empty");

    response = server.HandleRequest(
        "POST",
        "/api/playback/targets",
        R"({"targets":[{"id":"udp-a","name":"UDP A","protocol":"UDP","enabled":true,"endpoint":"239.1.1.1:5000"},{"id":"tcp-a","name":"TCP A","protocol":"TCP","enabled":false,"endpoint":"127.0.0.1:9000"}]})");
    Expect(response.status_code == 200, "set playback targets route should return 200");
    Expect(response.body.find("\"id\":\"udp-a\"") != std::string::npos,
           "set playback targets route should persist target ids");
    Expect(response.body.find("\"endpoint\":\"239.1.1.1:5000\"") != std::string::npos,
           "set playback targets route should echo endpoints");
    Expect(session.replay_targets.size() == 2,
           "set playback targets route should update session target state");

    response = server.HandleRequest("GET", "/api/logs", "");
    Expect(response.status_code == 200, "logs route should return 200");
    Expect(response.body.find("\"entries\":[]") != std::string::npos,
           "logs route should start empty before websocket events are recorded");

    stats.snapshot.total_packets = 3;
    stats.snapshot.total_drops = 1;
    if (stats.callback) {
        stats.callback(stats.snapshot);
    }
    if (session.callback) {
        session.callback(SessionState::Idle, SessionState::Playing);
    }

    response = server.HandleRequest("GET", "/api/logs", "");
    Expect(response.body.find("\"source\":\"stats\"") != std::string::npos,
           "logs route should include stats events");
    Expect(response.body.find("\"source\":\"session\"") != std::string::npos,
           "logs route should include session events");
    Expect(response.body.find("Idle -> Playing") != std::string::npos,
           "logs route should include state transition message");

    response = server.HandleRequest("GET", "/api/mappings", "");
    Expect(response.status_code == 200, "mappings route should return 200");
    Expect(response.body.find("\"source_topic\":\"/in/radar\"") != std::string::npos,
           "mappings route should include source topic");

    response = server.HandleRequest(
        "POST",
        "/api/mappings",
        R"({"mappings":[{"source_topic":"/in/camera","target_topic":"/out/camera"}]})");
    Expect(response.status_code == 200, "set mappings route should return 200");
    Expect(runtime.set_mappings_called, "set mappings route should invoke runtime catalog");
    Expect(runtime.mappings.size() == 1, "set mappings route should replace mappings");
    Expect(runtime.mappings[0].target_topic == "/out/camera",
           "set mappings route should forward target topic");
#else
    Expect(response.status_code == 500,
           "plugins route should report missing JSON support when nlohmann_json is unavailable");

    response = server.HandleRequest("GET", "/api/plugins/UDP", "");
    Expect(response.status_code == 500,
           "plugin detail route should report missing JSON support when nlohmann_json is unavailable");

    response = server.HandleRequest(
        "POST",
        "/api/plugins/UDP/config",
        R"({"config_fields":[{"key":"address","label":"Address","value":"239.9.9.9"}]})");
    Expect(response.status_code == 500,
           "plugin config route should report missing JSON support when nlohmann_json is unavailable");
    Expect(runtime.saved_plugin_id.empty(),
           "plugin config should not be persisted without JSON support");

    response = server.HandleRequest("POST", "/api/plugins/UDP/start", "");
    Expect(response.status_code == 500,
           "plugin start route should surface missing JSON support in its detail response");
    Expect(runtime.started_plugin_id == "UDP",
           "plugin start should still target the selected plugin before detail serialization fails");

    response = server.HandleRequest("POST", "/api/plugins/UDP/stop", "");
    Expect(response.status_code == 500,
           "plugin stop route should surface missing JSON support in its detail response");
    Expect(runtime.stopped_plugin_id == "UDP",
           "plugin stop should still target the selected plugin before detail serialization fails");

    response = server.HandleRequest("GET", "/api/channels", "");
    Expect(response.status_code == 500,
           "channels route should report missing JSON support when nlohmann_json is unavailable");

    response = server.HandleRequest("GET", "/api/mappings", "");
    Expect(response.status_code == 500,
           "mappings route should report missing JSON support when nlohmann_json is unavailable");

    response = server.HandleRequest(
        "POST",
        "/api/mappings",
        R"({"mappings":[{"source_topic":"/in/camera","target_topic":"/out/camera"}]})");
    Expect(response.status_code == 500,
           "set mappings route should report missing JSON support when nlohmann_json is unavailable");
    Expect(!runtime.set_mappings_called,
           "set mappings should not invoke the runtime catalog without JSON support");
#endif
    websocket.Stop();
}

void TestRuntimeRoutesRequireRuntimeCatalog() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest("GET", "/api/plugins", "");
    Expect(response.status_code == 503, "plugins route should return 503 without runtime catalog");

    response = server.HandleRequest("GET", "/api/channels", "");
    Expect(response.status_code == 503, "channels route should return 503 without runtime catalog");

    response = server.HandleRequest("GET", "/api/mappings", "");
    Expect(response.status_code == 503, "mappings route should return 503 without runtime catalog");
}

void TestUnknownRouteResponse() {
    HttpServer server(nullptr, nullptr);
    const auto response = server.HandleRequest("GET", "/api/unknown", "");
    Expect(response.status_code == 404, "unknown route should return 404");
}

void TestStaticFileRoutes() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_http_server_static_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "assets");

    {
        std::ofstream indexFile(tempRoot / "index.html", std::ios::binary);
        indexFile << "<!doctype html><html><body>RecPlay</body></html>";
    }
    {
        std::ofstream assetFile(tempRoot / "assets" / "app.js", std::ios::binary);
        assetFile << "console.log('recplay');";
    }

    HttpServer server(nullptr, nullptr);
    Expect(server.SetStaticRoot(tempRoot.string()), "static root should be accepted");

    auto response = server.HandleRequest("GET", "/", "");
    Expect(response.status_code == 200, "root path should serve index.html");
    Expect(response.content_type == "text/html", "index should use html content type");
    Expect(response.body.find("RecPlay") != std::string::npos, "index body should be served");

    response = server.HandleRequest("GET", "/assets/app.js", "");
    Expect(response.status_code == 200, "asset path should serve file");
    Expect(response.content_type == "application/javascript",
           "js asset should use javascript content type");
    Expect(response.body.find("console.log") != std::string::npos, "asset body should be served");

    response = server.HandleRequest("GET", "/dashboard", "");
    Expect(response.status_code == 200, "spa route should fall back to index.html");
    Expect(response.content_type == "text/html", "spa route should use html content type");

    response = server.HandleRequest("GET", "/assets/missing.js", "");
    Expect(response.status_code == 404, "missing asset should return 404");

    std::filesystem::remove_all(tempRoot);
}

void TestStaticFileRoutesRejectOversizedAssets() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_http_server_large_static_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "assets");

    {
        std::ofstream indexFile(tempRoot / "index.html", std::ios::binary);
        indexFile << "<!doctype html><html><body>RecPlay</body></html>";
    }
    {
        std::ofstream assetFile(tempRoot / "assets" / "big.bin", std::ios::binary);
        assetFile.seekp((16 * 1024 * 1024), std::ios::beg);
        assetFile.put('\0');
    }

    HttpServer server(nullptr, nullptr);
    Expect(server.SetStaticRoot(tempRoot.string()), "static root should be accepted for large asset");

    const auto response = server.HandleRequest("GET", "/assets/big.bin", "");
    Expect(response.status_code == 413, "oversized static asset should be rejected");

    std::filesystem::remove_all(tempRoot);
}

void TestStaticFileRoutesAllowWindowsCaseDifferences() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_http_server_case_static_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "Assets");

    {
        std::ofstream indexFile(tempRoot / "index.html", std::ios::binary);
        indexFile << "<!doctype html><html><body>RecPlay</body></html>";
    }
    {
        std::ofstream assetFile(tempRoot / "Assets" / "Case.js", std::ios::binary);
        assetFile << "console.log('case');";
    }

    HttpServer server(nullptr, nullptr);
    Expect(server.SetStaticRoot(tempRoot.string()), "static root should be accepted for case test");

    const auto response = server.HandleRequest("GET", "/Assets/Case.js", "");
    Expect(response.status_code == 200, "static path should serve file even if canonical casing differs");

    std::filesystem::remove_all(tempRoot);
}

void TestWebRootResolution() {
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "recplay_web_root_locator_test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot / "build" / "web");
    std::filesystem::create_directories(tempRoot / "source" / "web" / "dist");

    {
        std::ofstream buildIndex(tempRoot / "build" / "web" / "index.html", std::ios::binary);
        buildIndex << "build";
    }
    {
        std::ofstream sourceIndex(tempRoot / "source" / "web" / "dist" / "index.html", std::ios::binary);
        sourceIndex << "source";
    }

    const auto fromBuild = ResolveWebRoot(
        (tempRoot / "build" / "Debug").string(),
        (tempRoot / "source").string());
    Expect(fromBuild == std::filesystem::weakly_canonical(tempRoot / "build" / "web").string(),
           "resolver should prefer build web output");

    const auto fromBuildRoot = ResolveWebRoot(
        (tempRoot / "build").string(),
        (tempRoot / "source").string());
    Expect(fromBuildRoot == std::filesystem::weakly_canonical(tempRoot / "build" / "web").string(),
           "resolver should accept build root directly");

    std::filesystem::remove(tempRoot / "build" / "web" / "index.html");
    const auto fromSource = ResolveWebRoot(
        (tempRoot / "build" / "Debug").string(),
        (tempRoot / "source").string());
    Expect(fromSource == std::filesystem::weakly_canonical(tempRoot / "source" / "web" / "dist").string(),
           "resolver should fall back to source web dist");

    std::filesystem::remove_all(tempRoot);
}

void TestUnavailableServicesReturn503() {
    HttpServer server(nullptr, nullptr);

    auto response = server.HandleRequest("GET", "/api/session/state", "");
    Expect(response.status_code == 503, "session route should return 503 without service");

    response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 503, "stats route should return 503 without service");

    FakeSessionService session;
    FakeStatsService stats;
    server.SetSessionService(&session);
    server.SetStatsService(&stats);

    response = server.HandleRequest("GET", "/api/session/state", "");
    Expect(response.status_code == 200, "session route should recover after injection");

    response = server.HandleRequest("GET", "/api/stats", "");
    Expect(response.status_code == 200, "stats route should recover after injection");
}

void TestPlaybackCommandRoutes() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest("POST", "/api/session/playback/play", R"({"speed":1.5})");
    Expect(response.status_code == 200,
           "play route should return 200: code=" + std::to_string(response.status_code) +
           ", body=" + response.body);
    Expect(session.play_called, "play route should call session service");
    Expect(session.last_play_speed == 1.5, "play route should forward speed");

    response = server.HandleRequest("POST", "/api/session/playback/pause", "");
    Expect(response.status_code == 200, "pause route should return 200");
    Expect(session.pause_called, "pause route should call session service");

    response = server.HandleRequest("POST", "/api/session/playback/seek", R"({"timestamp_ns":400})");
    Expect(response.status_code == 200, "seek route should return 200");
    Expect(session.seek_called, "seek route should call session service");
    Expect(session.last_seek_ns == 400, "seek route should forward timestamp");

    response = server.HandleRequest("POST", "/api/session/playback/play", R"({"speed":1e3})");
    Expect(response.status_code == 200, "play route should accept exponent-form speed");
    Expect(session.last_play_speed == 1000.0, "play route should forward exponent-form speed");

    response = server.HandleRequest("POST", "/api/session/playback/seek", R"({"timestamp_ns":1e3})");
    Expect(response.status_code == 400, "seek route should reject exponent-form timestamp");
    Expect(session.last_seek_ns == 400, "seek route should not update timestamp on exponent-form input");

    response = server.HandleRequest("POST", "/api/session/playback/stop", "");
    Expect(response.status_code == 200, "stop route should return 200");
    Expect(session.stop_called, "stop route should call session service");
}

void TestPlaybackValidationErrors() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"timestamp_ns":"soon"})");
    Expect(response.status_code == 400, "seek route should reject non-numeric timestamp");
    Expect(!session.seek_called, "seek route should not call session service on invalid timestamp");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"timestamp_ns":-1})");
    Expect(response.status_code == 400, "seek route should reject negative timestamp");
    Expect(!session.seek_called, "seek route should not call session service on negative timestamp");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/play",
        R"({"speed":"fast"})");
    Expect(response.status_code == 400, "play route should reject non-numeric speed");
    Expect(!session.play_called, "play route should not call session service on invalid speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"speed":"fast"})");
    Expect(response.status_code == 400, "speed route should reject non-numeric speed");
    Expect(!session.set_speed_called, "speed route should not call session service on invalid speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"speed":1e3})");
    Expect(response.status_code == 200, "speed route should accept exponent-form speed");
    Expect(session.speed == 1000.0, "speed route should forward exponent-form speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":"early","end_ns":300})");
    Expect(response.status_code == 400, "loop route should reject non-numeric start");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":-1,"end_ns":300})");
    Expect(response.status_code == 400, "loop route should reject negative start");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":100,"end_ns":-1})");
    Expect(response.status_code == 400, "loop route should reject negative end");
}

void TestPlaybackNumericRoutesIgnoreSubstringAndNestedKeys() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/play",
        R"({"note":"speed","other":3.5})");
    Expect(response.status_code == 200, "play route should ignore speed token in string values");
    Expect(session.play_called, "play route should still call session service when speed is omitted");
    Expect(session.last_play_speed == 1.0, "play route should keep default speed when top-level speed is absent");

    session.play_called = false;
    session.last_play_speed = 0.0;
    response = server.HandleRequest(
        "POST",
        "/api/session/playback/play",
        R"({"playback":{"speed":2.5}})");
    Expect(response.status_code == 200, "play route should ignore nested speed fields");
    Expect(session.play_called, "play route should still call session service when nested speed is ignored");
    Expect(session.last_play_speed == 1.0, "play route should keep default speed when only nested speed exists");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"note":"speed","other":4.0})");
    Expect(response.status_code == 400, "speed route should reject missing top-level speed");
    Expect(!session.set_speed_called, "speed route should not use a sibling field after a string token match");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/speed",
        R"({"playback":{"speed":4.0}})");
    Expect(response.status_code == 400, "speed route should reject nested speed");
    Expect(!session.set_speed_called, "speed route should not use nested speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"note":"timestamp_ns","other":400})");
    Expect(response.status_code == 400, "seek route should reject missing top-level timestamp");
    Expect(!session.seek_called, "seek route should not use a sibling field after a string token match");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/seek",
        R"({"playback":{"timestamp_ns":400}})");
    Expect(response.status_code == 400, "seek route should reject nested timestamp");
    Expect(!session.seek_called, "seek route should not use nested timestamp");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"note":"start_ns","other":100,"end_ns":300})");
    Expect(response.status_code == 400, "loop route should reject missing top-level start");
    Expect(!session.loop_called, "loop route should not use a sibling field after a string token match");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"playback":{"start_ns":100,"end_ns":300}})");
    Expect(response.status_code == 400, "loop route should reject nested loop bounds");
    Expect(!session.loop_called, "loop route should not use nested loop bounds");
}

void TestPlaybackInvalidStateResponses() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    session.state = SessionState::Idle;

    const auto response = server.HandleRequest("POST", "/api/session/playback/pause", "");
    Expect(response.status_code == 409, "pause route should reject idle playback state");
    Expect(!session.pause_called, "pause route should not call session service when state is idle");
}

void TestRemainingSessionRoutes() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    auto response = server.HandleRequest(
        "POST",
        "/api/session/record",
        R"({"output_path":"capture.rpcap","protocols":["UDP"]})");
    Expect(response.status_code == 200,
           "record route should return 200: code=" + std::to_string(response.status_code) +
           ", body=" + response.body);
    Expect(session.start_record_called, "record route should call StartRecording");
    Expect(session.last_record_config.find("\"output_path\":\"capture.rpcap\"") != std::string::npos,
           "record route should forward config body");

    response = server.HandleRequest("POST", "/api/session/record/pause", "");
    Expect(response.status_code == 200, "record pause route should return 200");
    Expect(session.pause_record_called, "record pause should call PauseRecording");

    response = server.HandleRequest("POST", "/api/session/record/resume", "");
    Expect(response.status_code == 200, "record resume route should return 200");
    Expect(session.resume_record_called, "record resume should call ResumeRecording");

    response = server.HandleRequest("POST", "/api/session/record/stop", "");
    Expect(response.status_code == 200, "record stop route should return 200");
    Expect(session.stop_record_called, "record stop should call StopRecording");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/open",
        R"({"file_path":"capture.rpcap"})");
    Expect(response.status_code == 200, "playback open route should return 200");
    Expect(session.open_playback_called, "playback open should call OpenForPlayback");
    Expect(session.last_open_path == "capture.rpcap", "playback open should forward file path");

    response = server.HandleRequest("POST", "/api/session/playback/speed", R"({"speed":4.0})");
    Expect(response.status_code == 200, "playback speed route should return 200");
    Expect(session.set_speed_called, "playback speed should call SetSpeed");
    Expect(session.speed == 4.0, "playback speed should forward speed");

    response = server.HandleRequest(
        "POST",
        "/api/session/playback/loop",
        R"({"start_ns":100,"end_ns":300})");
    Expect(response.status_code == 200, "playback loop route should return 200");
}

void TestErrorResponseEscapesJsonMessage() {
    class FailingSessionService final : public ISessionService {
    public:
        SessionState GetState() const override { return SessionState::Idle; }
        void OnStateChanged(StateCallback cb) override { callback = std::move(cb); }
        bool StartRecording(const std::string&) override { return false; }
        void PauseRecording() override {}
        void ResumeRecording() override {}
        void StopRecording() override {}
        bool OpenForPlayback(const std::string& path, const std::string& = "{}") override {
            last_open_path = path;
            return false;
        }
        void Play(double) override {}
        void Pause() override {}
        void SeekTo(uint64_t) override {}
        void SetSpeed(double) override {}
        void SetLoopRange(uint64_t, uint64_t) override {}
        void Stop() override {}
        void Reset() override {}
        uint64_t GetDuration() const override { return 0; }
        uint64_t GetCurrentPosition() const override { return 0; }
        double GetCurrentSpeed() const override { return 1.0; }
        PlaybackPacketSnapshot GetCurrentPlaybackPacket() const override { return {}; }
        std::vector<ReplayTarget> GetReplayTargets() const override { return {}; }
        void SetReplayTargets(const std::vector<ReplayTarget>&) override {}

        StateCallback callback;
        std::string last_open_path;
    };

    FailingSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/open",
        R"({"file_path":"broken\"name.rpcap"})");

    Expect(response.status_code == 400, "failed playback open should return 400");
    Expect(session.last_open_path == "broken\"name.rpcap",
           "playback open should decode escaped quotes before forwarding file path");
    Expect(response.body.find("\\\"") != std::string::npos,
           "error response should escape embedded quotes in JSON body: " + response.body);
}

void TestPlaybackOpenDecodesEscapedWindowsPath() {
    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    const auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/open",
        R"({"file_path":"C:\\captures\\demo.rpcap"})");

    Expect(response.status_code == 200, "playback open should accept escaped Windows-style paths");
    Expect(session.last_open_path == "C:\\captures\\demo.rpcap",
           "playback open should unescape Windows-style path separators");
}

void TestPlaybackOpenResolvesBasenameAgainstRecordingsDirectory() {
    namespace fs = std::filesystem;

    FakeSessionService session;
    FakeStatsService stats;
    HttpServer server(&session, &stats);

    const fs::path temp_root = fs::temp_directory_path() / "recplay-open-file-test";
    const fs::path recordings_dir = temp_root / "captures";
    std::error_code ec;
    fs::create_directories(recordings_dir, ec);
    Expect(!ec, "should create temp recordings directory for playback open");

    const fs::path recording_path = recordings_dir / "flight1.rpcap";
    {
        std::ofstream file(recording_path, std::ios::binary);
        Expect(file.good(), "should create temp playback file");
        file << "RPCP";
    }

    server.SetRecordingsDirectory(recordings_dir.string());
    const auto response = server.HandleRequest(
        "POST",
        "/api/session/playback/open",
        R"({"file_path":"flight1.rpcap"})");

    Expect(response.status_code == 200, "playback open should resolve basename relative to recordings dir");
    Expect(session.last_open_path == recording_path.string(),
           "playback open should forward resolved full path from recordings dir");

    fs::remove_all(temp_root, ec);
}

void TestFilesRouteIncludesFullPathForPlaybackRoundTrip() {
    namespace fs = std::filesystem;

    FakeSessionService session;
    FakeStatsService stats;
    FakeStorageService storage;
    RecordingFileInfo fileInfo;
    fileInfo.duration_ns = 123;
    fileInfo.total_packets = 7;
    fileInfo.creation_time = 456;
    fileInfo.channel_count = 2;
    storage.file_info = fileInfo;

    const fs::path temp_root = fs::temp_directory_path() / "recplay-http-files-test";
    const fs::path recordings_dir = temp_root / "captures";
    std::error_code ec;
    fs::create_directories(recordings_dir, ec);
    Expect(!ec, "should create temp recordings directory");

    const fs::path recording_path = recordings_dir / "flight1.rpcap";
    {
        std::ofstream file(recording_path, std::ios::binary);
        Expect(file.good(), "should create temp recording");
        file << "RPCP";
    }

    HttpServer server(&session, &stats);
    server.SetStorageService(&storage);
    server.SetRecordingsDirectory(recordings_dir.string());

    const auto response = server.HandleRequest("GET", "/api/files", "");
    Expect(response.status_code == 200, "files route should return 200");
    Expect(response.body.find("\"name\":\"flight1.rpcap\"") != std::string::npos,
           "files route should include file name: " + response.body);
    Expect(response.body.find("\"path\":\"") != std::string::npos,
           "files route should include full path for playback round-trip: " + response.body);
    Expect(storage.last_probe_path == recording_path.string(),
           "storage probe should receive full path");

    fs::remove_all(temp_root, ec);
}

void TestRunningStateTransitions() {
    HttpServer server(nullptr, nullptr);
    Expect(!server.IsRunning(), "server should start stopped");
    Expect(server.Start("127.0.0.1", 0), "start should succeed");
    Expect(server.IsRunning(), "server should report running after start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after stop");

    Expect(server.Start("127.0.0.1", 0), "second start should succeed");
    Expect(server.IsRunning(), "server should report running after second start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after second stop");
}

void TestSecondHttpServerInstanceCannotStartWhileFirstIsActive() {
    HttpServer first(nullptr, nullptr);
    HttpServer second(nullptr, nullptr);

    Expect(first.Start("127.0.0.1", 0), "first server should start");
    Expect(!second.Start("127.0.0.1", 0),
           "second server instance should fail while another server is active");

    first.Stop();
}

} // namespace

int main() {
    try {
        TestSessionStateResponse();
        TestStatsSnapshotResponse();
        TestStatsSnapshotResponseSanitizesNonFiniteNumbers();
        TestRuntimeRoutes();
        TestRuntimeRoutesRequireRuntimeCatalog();
        TestUnknownRouteResponse();
        TestStaticFileRoutes();
        TestStaticFileRoutesRejectOversizedAssets();
        TestStaticFileRoutesAllowWindowsCaseDifferences();
        TestWebRootResolution();
        TestUnavailableServicesReturn503();
        TestPlaybackCommandRoutes();
        TestPlaybackValidationErrors();
        TestPlaybackNumericRoutesIgnoreSubstringAndNestedKeys();
        TestPlaybackInvalidStateResponses();
        TestRemainingSessionRoutes();
        TestErrorResponseEscapesJsonMessage();
        TestPlaybackOpenDecodesEscapedWindowsPath();
        TestPlaybackOpenResolvesBasenameAgainstRecordingsDirectory();
        TestFilesRouteIncludesFullPathForPlaybackRoundTrip();
        TestRunningStateTransitions();
        TestSecondHttpServerInstanceCannotStartWhileFirstIsActive();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
