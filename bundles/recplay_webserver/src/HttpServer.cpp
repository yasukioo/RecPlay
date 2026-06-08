// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"

#include "DrogonWindowsCompat.h"
#include "IRuntimeCatalog.h"
#include "IStatsService.h"
#include "ISessionService.h"
#include "IStorageService.h"
#include "StaticFileHandler.h"
#include "WebSocketServer.h"

#include <cstdint>

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define RECPLAY_HAS_DROGON 1
#else
#define RECPLAY_HAS_DROGON 0
#endif

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#define RECPLAY_HTTP_SERVER_HAS_JSON 1
#else
#define RECPLAY_HTTP_SERVER_HAS_JSON 0
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <vector>

namespace recplay {

namespace {

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

bool SkipJsonString(const std::string& body, size_t* cursor) {
    if (cursor == nullptr || *cursor >= body.size() || body[*cursor] != '"') {
        return false;
    }

    ++(*cursor);
    while (*cursor < body.size()) {
        if (body[*cursor] == '\\') {
            *cursor += 2;
            continue;
        }
        if (body[*cursor] == '"') {
            ++(*cursor);
            return true;
        }
        ++(*cursor);
    }

    return false;
}

bool FindTopLevelKeyValueStart(const std::string& body, const std::string& key, size_t* valueStart) {
    if (valueStart == nullptr) {
        return false;
    }

    size_t cursor = 0;
    while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor])) != 0) {
        ++cursor;
    }
    if (cursor >= body.size() || body[cursor] != '{') {
        return false;
    }

    ++cursor;
    int depth = 1;
    while (cursor < body.size()) {
        while (cursor < body.size() &&
               std::isspace(static_cast<unsigned char>(body[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= body.size()) {
            break;
        }

        if (body[cursor] == '"') {
            const size_t keyStart = cursor;
            if (!SkipJsonString(body, &cursor)) {
                return false;
            }

            if (depth != 1) {
                continue;
            }

            const std::string candidate = body.substr(keyStart + 1, cursor - keyStart - 2);
            size_t probe = cursor;
            while (probe < body.size() &&
                   std::isspace(static_cast<unsigned char>(body[probe])) != 0) {
                ++probe;
            }
            if (probe >= body.size() || body[probe] != ':') {
                continue;
            }
            ++probe;
            while (probe < body.size() &&
                   std::isspace(static_cast<unsigned char>(body[probe])) != 0) {
                ++probe;
            }

            if (candidate == key) {
                *valueStart = probe;
                return true;
            }
            cursor = probe;
            continue;
        }

        if (body[cursor] == '{' || body[cursor] == '[') {
            ++depth;
            ++cursor;
            continue;
        }
        if (body[cursor] == '}' || body[cursor] == ']') {
            --depth;
            ++cursor;
            continue;
        }

        ++cursor;
    }

    return false;
}

bool JsonKeyExists(const std::string& body, const std::string& key) {
    size_t valueStart = 0;
    return FindTopLevelKeyValueStart(body, key, &valueStart);
}

bool TryParseDouble(const std::string& value, double* parsedValue) {
    if (parsedValue == nullptr) {
        return false;
    }

    const std::string trimmed = TrimCopy(value);
    if (trimmed.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const double result = std::strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || *end != '\0' || errno == ERANGE) {
        return false;
    }

    *parsedValue = result;
    return true;
}

bool TryParseUint64(const std::string& value, uint64_t* parsedValue) {
    if (parsedValue == nullptr) {
        return false;
    }

    const std::string trimmed = TrimCopy(value);
    if (trimmed.empty()) {
        return false;
    }
    if (trimmed.front() == '-') {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long long result = std::strtoull(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || *end != '\0' || errno == ERANGE) {
        return false;
    }

    *parsedValue = static_cast<uint64_t>(result);
    return true;
}

double SanitizeJsonNumber(double value) {
    return std::isfinite(value) ? value : 0.0;
}

std::string BuildReplayTargetEndpoint(const std::map<std::string, std::string>& config) {
    if (const auto endpointIt = config.find("endpoint"); endpointIt != config.end()) {
        return endpointIt->second;
    }

    std::string endpoint;
    if (const auto addressIt = config.find("address"); addressIt != config.end()) {
        endpoint = addressIt->second;
        if (const auto portIt = config.find("port"); portIt != config.end()) {
            endpoint += ":" + portIt->second;
        }
        return endpoint;
    }
    if (const auto hostIt = config.find("host"); hostIt != config.end()) {
        endpoint = hostIt->second;
        if (const auto portIt = config.find("port"); portIt != config.end()) {
            endpoint += ":" + portIt->second;
        }
    }
    return endpoint;
}

std::string ResolvePlaybackOpenPath(const std::string& filePath, const std::string& recordingsDir) {
    namespace fs = std::filesystem;

    if (filePath.empty()) {
        return {};
    }

    const fs::path input(filePath);
    if (input.is_absolute()) {
        return input.lexically_normal().string();
    }

    std::error_code ec;
    if (fs::exists(input, ec) && !ec) {
        return input.lexically_normal().string();
    }

    const fs::path candidate = fs::path(recordingsDir) / input;
    ec.clear();
    if (fs::exists(candidate, ec) && !ec) {
        return candidate.lexically_normal().string();
    }

    return input.lexically_normal().string();
}

std::string BytesToHex(const std::vector<uint8_t>& payload, std::size_t maxBytes = 256) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    const std::size_t count = std::min(payload.size(), maxBytes);
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0 && i % 16 == 0) {
            stream << '\n';
        } else if (i > 0) {
            stream << ' ';
        }
        stream << std::setw(2) << static_cast<unsigned int>(payload[i]);
    }
    return stream.str();
}

bool AppendUtf8CodePoint(uint32_t codePoint, std::string* output) {
    if (output == nullptr || codePoint > 0x10ffff ||
        (codePoint >= 0xd800 && codePoint <= 0xdfff)) {
        return false;
    }

    if (codePoint <= 0x7f) {
        output->push_back(static_cast<char>(codePoint));
        return true;
    }
    if (codePoint <= 0x7ff) {
        output->push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        output->push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
        return true;
    }
    if (codePoint <= 0xffff) {
        output->push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        output->push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
        return true;
    }

    output->push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
    output->push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    return true;
}

bool ReadJsonHexCodeUnit(const std::string& literal, size_t offset, uint32_t* codeUnit) {
    if (codeUnit == nullptr || offset + 4 > literal.size()) {
        return false;
    }

    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value <<= 4;
        const unsigned char ch = static_cast<unsigned char>(literal[offset + i]);
        if (ch >= '0' && ch <= '9') {
            value |= static_cast<uint32_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            value |= static_cast<uint32_t>(10 + ch - 'a');
        } else if (ch >= 'A' && ch <= 'F') {
            value |= static_cast<uint32_t>(10 + ch - 'A');
        } else {
            return false;
        }
    }

    *codeUnit = value;
    return true;
}

bool UnescapeJsonStringLiteral(const std::string& literal, std::string* output) {
    if (output == nullptr) {
        return false;
    }

#if RECPLAY_HTTP_SERVER_HAS_JSON
    try {
        *output = nlohmann::json::parse("\"" + literal + "\"").get<std::string>();
        return true;
    } catch (...) {
    }
#endif

    output->clear();
    for (size_t i = 0; i < literal.size(); ++i) {
        const char ch = literal[i];
        if (ch != '\\') {
            output->push_back(ch);
            continue;
        }
        if (i + 1 >= literal.size()) {
            return false;
        }

        const char escaped = literal[++i];
        switch (escaped) {
            case '"':
            case '\\':
            case '/':
                output->push_back(escaped);
                break;
            case 'b':
                output->push_back('\b');
                break;
            case 'f':
                output->push_back('\f');
                break;
            case 'n':
                output->push_back('\n');
                break;
            case 'r':
                output->push_back('\r');
                break;
            case 't':
                output->push_back('\t');
                break;
            case 'u': {
                uint32_t codePoint = 0;
                if (!ReadJsonHexCodeUnit(literal, i + 1, &codePoint)) {
                    return false;
                }
                i += 4;

                if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                    if (i + 6 >= literal.size() || literal[i + 1] != '\\' || literal[i + 2] != 'u') {
                        return false;
                    }
                    uint32_t lowSurrogate = 0;
                    if (!ReadJsonHexCodeUnit(literal, i + 3, &lowSurrogate) ||
                        lowSurrogate < 0xdc00 || lowSurrogate > 0xdfff) {
                        return false;
                    }
                    codePoint = 0x10000 +
                        (((codePoint - 0xd800) << 10) | (lowSurrogate - 0xdc00));
                    i += 6;
                } else if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
                    return false;
                }

                if (!AppendUtf8CodePoint(codePoint, output)) {
                    return false;
                }
                break;
            }
            default:
                return false;
        }
    }

    return true;
}

#if RECPLAY_HAS_DROGON
std::string RequestMethodToString(drogon::HttpMethod method) {
    switch (method) {
        case drogon::Get:
            return "GET";
        case drogon::Post:
            return "POST";
        default:
            return {};
    }
}
#endif

#if RECPLAY_HAS_DROGON
drogon::HttpStatusCode ToDrogonStatus(int statusCode) {
    return static_cast<drogon::HttpStatusCode>(statusCode);
}

drogon::HttpMethod ToDrogonMethod(const std::string& method) {
    if (method == "GET") {
        return drogon::Get;
    }
    if (method == "POST") {
        return drogon::Post;
    }
    return drogon::Invalid;
}

std::atomic<HttpServer*> g_active_http_server{nullptr};
std::mutex g_drogon_state_mutex;
bool g_drogon_listener_configured = false;
std::string g_drogon_listener_host;
int g_drogon_listener_port = -1;
std::once_flag g_drogon_thread_once;
std::thread g_drogon_thread;

HttpResponse MakeDrogonUnavailableResponse() {
    return HttpResponse{503, "application/json", "{\"error\":\"HTTP server unavailable\"}"};
}

void SendDrogonResponse(
    const HttpResponse& response,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto httpResponse = drogon::HttpResponse::newHttpResponse();
    httpResponse->setStatusCode(ToDrogonStatus(response.status_code));
    httpResponse->setContentTypeString(response.content_type);
    httpResponse->setBody(response.body);
    callback(httpResponse);
}

HttpResponse DispatchToActiveHttpServer(
    const std::string& method,
    const std::string& path,
    const std::string& body) {
    auto* server = g_active_http_server.load(std::memory_order_acquire);
    if (server == nullptr || !server->IsRunning()) {
        return MakeDrogonUnavailableResponse();
    }
    return server->HandleRequest(method, path, body);
}

void RegisterDrogonRoutes() {
    const auto registerRoute =
        [](const std::string& method, const std::string& path) {
            drogon::app().registerHandler(
                path,
                [method, path](const drogon::HttpRequestPtr& request,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                    SendDrogonResponse(
                        DispatchToActiveHttpServer(
                            method,
                            path,
                            std::string(request->getBody())),
                        std::move(callback));
                },
                {ToDrogonMethod(method)});
        };

    drogon::app().registerHandler(
        "/api/health",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto* server = g_active_http_server.load(std::memory_order_acquire);
            if (server == nullptr || !server->IsRunning()) {
                SendDrogonResponse(MakeDrogonUnavailableResponse(), std::move(callback));
                return;
            }

            auto response = drogon::HttpResponse::newHttpResponse();
            response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            response->setBody("{\"status\":\"ok\"}");
            callback(response);
        });

    registerRoute("GET", "/api/session/state");
    registerRoute("GET", "/api/stats");
    registerRoute("POST", "/api/session/record");
    registerRoute("POST", "/api/session/reset");
    registerRoute("POST", "/api/session/record/pause");
    registerRoute("POST", "/api/session/record/resume");
    registerRoute("POST", "/api/session/record/stop");
    registerRoute("POST", "/api/session/playback/open");
    registerRoute("POST", "/api/session/playback/play");
    registerRoute("POST", "/api/session/playback/pause");
    registerRoute("POST", "/api/session/playback/seek");
    registerRoute("POST", "/api/session/playback/speed");
    registerRoute("POST", "/api/session/playback/loop");
    registerRoute("POST", "/api/session/playback/stop");
    registerRoute("GET", "/api/plugins");
    registerRoute("GET", "/api/channels");
    registerRoute("GET", "/api/files");
    registerRoute("GET", "/api/markers");
    registerRoute("GET", "/api/playback/packet");
    registerRoute("GET", "/api/playback/density");
    registerRoute("GET", "/api/playback/targets");
    registerRoute("POST", "/api/playback/targets");
    registerRoute("GET", "/api/logs");
    registerRoute("GET", "/api/mappings");
    registerRoute("POST", "/api/mappings");

    drogon::app().registerHandler(
        "/api/plugins/{path:.*}",
        [](const drogon::HttpRequestPtr& request,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            SendDrogonResponse(
                DispatchToActiveHttpServer(
                    RequestMethodToString(request->method()),
                    request->path(),
                    std::string(request->getBody())),
                std::move(callback));
        },
        {drogon::Get, drogon::Post});

    drogon::app().registerHandlerViaRegex(
        "^/(?!api(?:/|$)).*$",
        [](const drogon::HttpRequestPtr& request,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            SendDrogonResponse(
                DispatchToActiveHttpServer(
                    "GET",
                    request->path(),
                    std::string(request->getBody())),
                std::move(callback));
        },
        {drogon::Get});
}

bool ConfigureDrogonApp(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(g_drogon_state_mutex);
    if (!g_drogon_listener_configured) {
        drogon::app().addListener(host, static_cast<uint16_t>(port));
        RegisterDrogonRoutes();
        g_drogon_listener_configured = true;
        g_drogon_listener_host = host;
        g_drogon_listener_port = port;
        return true;
    }

    return g_drogon_listener_host == host && g_drogon_listener_port == port;
}

void ShutdownDrogonOnProcessExit() {
    std::thread threadToJoin;
    {
        std::lock_guard<std::mutex> lock(g_drogon_state_mutex);
        g_active_http_server.store(nullptr, std::memory_order_release);
        if (g_drogon_thread.joinable()) {
            threadToJoin = std::move(g_drogon_thread);
        }
    }

    try {
        if (drogon::app().isRunning()) {
            drogon::app().quit();
        }
    } catch (...) {
    }

    if (threadToJoin.joinable()) {
        threadToJoin.detach();
    }
}

bool EnsureDrogonThreadRunning() {
    std::call_once(g_drogon_thread_once, [] {
        std::atexit(ShutdownDrogonOnProcessExit);
        g_drogon_thread = std::thread([] {
            try {
                drogon::app().run();
            } catch (...) {
            }
        });
    });

    for (size_t attempt = 0; attempt < 200; ++attempt) {
        if (drogon::app().isRunning()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    try {
        return drogon::app().isRunning();
    } catch (...) {
        return false;
    }
}
#endif

} // namespace

HttpServer::HttpServer(ISessionService* session,
                       IStatsService* stats,
                       IRuntimeCatalog* runtimeCatalog)
    : session_(session), stats_(stats), runtime_catalog_(runtimeCatalog) {}

bool HttpServer::Start(const std::string& host, int port) {
    if (running_.exchange(true)) {
        return true;
    }

#if RECPLAY_HAS_DROGON
    try {
        auto* active = g_active_http_server.load(std::memory_order_acquire);
        if (active != nullptr && active != this) {
            running_.store(false, std::memory_order_release);
            return false;
        }
        if (!ConfigureDrogonApp(host, port) || !EnsureDrogonThreadRunning()) {
            running_.store(false, std::memory_order_release);
            return false;
        }
        HttpServer* expected = nullptr;
        if (!g_active_http_server.compare_exchange_strong(
                expected,
                this,
                std::memory_order_acq_rel,
                std::memory_order_acquire) &&
            expected != this) {
            running_.store(false, std::memory_order_release);
            return false;
        }
    } catch (...) {
        running_.store(false, std::memory_order_release);
        return false;
    }
#else
    (void)host;
    (void)port;
#endif
    return true;
}

void HttpServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

#if RECPLAY_HAS_DROGON
    HttpServer* expected = this;
    g_active_http_server.compare_exchange_strong(
        expected,
        nullptr,
        std::memory_order_acq_rel,
        std::memory_order_acquire);
#endif
}

bool HttpServer::IsRunning() const {
    return running_;
}

void HttpServer::SetSessionService(ISessionService* session) {
    session_.store(session, std::memory_order_release);
}

void HttpServer::SetStatsService(IStatsService* stats) {
    stats_.store(stats, std::memory_order_release);
}

void HttpServer::SetRuntimeCatalog(IRuntimeCatalog* runtimeCatalog) {
    runtime_catalog_.store(runtimeCatalog, std::memory_order_release);
}

void HttpServer::SetStorageService(IStorageService* storage) {
    storage_.store(storage, std::memory_order_release);
}

void HttpServer::SetWebSocketServer(WebSocketServer* webSocketServer) {
    websocket_server_.store(webSocketServer, std::memory_order_release);
}

bool HttpServer::SetStaticRoot(const std::string& root) {
    return static_files_.SetRoot(root);
}

void HttpServer::SetRecordingsDirectory(const std::string& directory) {
    if (!directory.empty()) {
        recordings_dir_ = directory;
    }
}

HttpResponse HttpServer::HandleRequest(const std::string& method,
                                       const std::string& path,
                                       const std::string& body) const {
    if (method == "GET" && path == "/api/session/state") {
        return HandleSessionState();
    }
    if (method == "GET" && path == "/api/stats") {
        return HandleStats();
    }
    if (method == "POST" && path == "/api/session/record") {
        return HandleRecordStart(body);
    }
    if (method == "POST" && path == "/api/session/reset") {
        return HandleSessionReset();
    }
    if (method == "POST" && path == "/api/session/record/pause") {
        return HandleRecordPause();
    }
    if (method == "POST" && path == "/api/session/record/resume") {
        return HandleRecordResume();
    }
    if (method == "POST" && path == "/api/session/record/stop") {
        return HandleRecordStop();
    }
    if (method == "POST" && path == "/api/session/playback/open") {
        return HandlePlaybackOpen(body);
    }
    if (method == "POST" && path == "/api/session/playback/play") {
        return HandlePlaybackPlay(body);
    }
    if (method == "POST" && path == "/api/session/playback/pause") {
        return HandlePlaybackPause();
    }
    if (method == "POST" && path == "/api/session/playback/seek") {
        return HandlePlaybackSeek(body);
    }
    if (method == "POST" && path == "/api/session/playback/speed") {
        return HandlePlaybackSpeed(body);
    }
    if (method == "POST" && path == "/api/session/playback/loop") {
        return HandlePlaybackLoop(body);
    }
    if (method == "POST" && path == "/api/session/playback/stop") {
        return HandlePlaybackStop();
    }
    if (method == "GET" && path == "/api/plugins") {
        return HandlePlugins();
    }
    if (method == "GET" && path == "/api/channels") {
        return HandleChannels();
    }
    if (method == "GET" && path == "/api/files") {
        return HandleFiles();
    }
    if (method == "GET" && path == "/api/markers") {
        return HandleMarkers();
    }
    if (method == "GET" && path == "/api/playback/packet") {
        return HandlePlaybackPacket();
    }
    if (method == "GET" && path == "/api/playback/density") {
        return HandlePlaybackDensity();
    }
    if (method == "GET" && path == "/api/playback/targets") {
        return HandleReplayTargets();
    }
    if (method == "POST" && path == "/api/playback/targets") {
        return HandleSetReplayTargets(body);
    }
    if (method == "GET" && path == "/api/logs") {
        return HandleLogs();
    }
    if (method == "GET" && path == "/api/mappings") {
        return HandleMappings();
    }
    if (method == "POST" && path == "/api/mappings") {
        return HandleSetMappings(body);
    }
    if (path.rfind("/api/plugins/", 0) == 0) {
        const std::string pluginPath = path.substr(std::string("/api/plugins/").size());
        const auto separator = pluginPath.find('/');
        const std::string pluginId = separator == std::string::npos
            ? pluginPath
            : pluginPath.substr(0, separator);
        const std::string action = separator == std::string::npos
            ? std::string{}
            : pluginPath.substr(separator + 1);

        if (pluginId.empty()) {
            return MakeErrorResponse(404, "Not Found");
        }
        if (method == "GET" && action.empty()) {
            return HandlePluginDetail(pluginId);
        }
        if (method == "POST" && action == "config") {
            return HandlePluginConfig(pluginId, body);
        }
        if (method == "POST" && action == "start") {
            return HandlePluginStart(pluginId);
        }
        if (method == "POST" && action == "stop") {
            return HandlePluginStop(pluginId);
        }
    }

    if (method == "GET" && path.rfind("/api/", 0) != 0) {
        return static_files_.Handle(path);
    }

    return MakeErrorResponse(404, "Not Found");
}

HttpResponse HttpServer::HandleSessionState() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    std::ostringstream stream;
    stream << "{\"state\":\"" << SessionStateToString(session->GetState()) << "\""
           << ",\"duration_ns\":" << session->GetDuration()
           << ",\"position_ns\":" << session->GetCurrentPosition()
           << ",\"speed\":" << SanitizeJsonNumber(session->GetCurrentSpeed())
           << "}";
    return MakeJsonResponse(stream.str());
}

HttpResponse HttpServer::HandleSessionReset() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session->Reset();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleStats() const {
    auto* stats = stats_.load(std::memory_order_acquire);
    if (stats == nullptr) {
        return MakeErrorResponse(503, "Stats service unavailable");
    }

    const auto snapshot = stats->GetSnapshot();
    std::ostringstream stream;
    stream << "{\"total_throughput_mbps\":" << SanitizeJsonNumber(snapshot.total_throughput_mbps)
           << ",\"total_packets\":" << snapshot.total_packets
           << ",\"total_drops\":" << snapshot.total_drops
           << ",\"total_bytes\":" << snapshot.total_bytes
           << ",\"drop_rate\":" << SanitizeJsonNumber(snapshot.drop_rate)
           << ",\"write_latency_p99_ms\":" << SanitizeJsonNumber(snapshot.write_latency_p99_ms)
           << ",\"ringbuf_used\":" << snapshot.ringbuf_used
           << ",\"ringbuf_capacity\":" << snapshot.ringbuf_capacity
           << ",\"disk_queue_bytes\":" << snapshot.disk_queue_bytes
           << ",\"cpu_usage_percent\":";
    if (snapshot.cpu_usage_percent.has_value() &&
        std::isfinite(*snapshot.cpu_usage_percent)) {
        stream << *snapshot.cpu_usage_percent;
    } else {
        stream << "null";
    }

    // Per-protocol / per-channel breakdown so the workbenches can show channel
    // throughput + drops (StatsCollector already aggregates these). Emitted as
    // JSON objects keyed by protocol name / channel id to match the frontend
    // StatsSnapshot.per_protocol / per_channel Record<string, …> shape.
    stream << ",\"per_protocol\":{";
    bool firstProtocol = true;
    for (const auto& [name, ps] : snapshot.per_protocol) {
        if (!firstProtocol) {
            stream << ",";
        }
        firstProtocol = false;
        stream << "\"" << EscapeJsonString(name) << "\":{"
               << "\"throughput_mbps\":" << SanitizeJsonNumber(ps.throughput_mbps)
               << ",\"packets\":" << ps.packets
               << ",\"drops\":" << ps.drops << "}";
    }
    stream << "},\"per_channel\":{";
    bool firstChannel = true;
    for (const auto& [channelId, cs] : snapshot.per_channel) {
        if (!firstChannel) {
            stream << ",";
        }
        firstChannel = false;
        stream << "\"" << channelId << "\":{"
               << "\"throughput_mbps\":" << SanitizeJsonNumber(cs.throughput_mbps)
               << ",\"packets\":" << cs.packets << "}";
    }
    stream << "}}";
    return MakeJsonResponse(stream.str());
}

HttpResponse HttpServer::HandleRecordStart(const std::string& body) const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    // Resolve a relative output_path against the recordings directory so that
    // bare filenames like "capture.rpcap" land in the configured data folder
    // regardless of the process working directory.
    std::string resolvedBody = body;
#if RECPLAY_HTTP_SERVER_HAS_JSON
    try {
        auto document = nlohmann::json::parse(body.empty() ? "{}" : body);
        const auto rawPath = document.value("output_path", std::string{});
        if (!rawPath.empty() && !recordings_dir_.empty()) {
            namespace fs = std::filesystem;
            const fs::path input(rawPath);
            if (!input.is_absolute()) {
                document["output_path"] =
                    (fs::path(recordings_dir_) / input).lexically_normal().string();
                resolvedBody = document.dump();
            }
        }
    } catch (...) {}
#endif

    if (!session->StartRecording(resolvedBody)) {
        return MakeErrorResponse(400, "Failed to start recording");
    }
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleRecordPause() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session->PauseRecording();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleRecordResume() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session->ResumeRecording();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleRecordStop() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session->StopRecording();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackOpen(const std::string& body) const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    const auto filePath = ExtractJsonStringLiteral(body, "file_path");
    if (filePath.empty()) {
        return MakeErrorResponse(400, "file_path is required");
    }
    const auto resolvedFilePath = ResolvePlaybackOpenPath(filePath, recordings_dir_);

    // Optional replay target config (UDP/TCP address+port to re-emit packets to).
    // Falls back to "protocol", then to "{}" (protocol service defaults).
    std::string replayConfig = ExtractJsonObjectLiteral(body, "protocol_config");
    if (replayConfig.empty()) {
        replayConfig = ExtractJsonObjectLiteral(body, "protocol");
    }
    if (replayConfig.empty()) {
        replayConfig = "{}";
    }

    if (!session->OpenForPlayback(resolvedFilePath, replayConfig)) {
        BroadcastReplayTargetsIfAvailable();
        return MakeErrorResponse(400, "Failed to open playback file: " + resolvedFilePath);
    }
    BroadcastReplayTargetsIfAvailable();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackPlay(const std::string& body) const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    double speed = 1.0;
    const auto value = ExtractJsonNumberLiteral(body, "speed");
    if (!value.empty()) {
        if (!TryParseDouble(value, &speed)) {
            return MakeErrorResponse(400, "speed must be numeric");
        }
    } else if (JsonKeyExists(body, "speed")) {
        return MakeErrorResponse(400, "speed must be numeric");
    }
    session->Play(speed);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackPause() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    if (session->GetState() != SessionState::Playing) {
        return MakeErrorResponse(409, "Playback is not currently playing");
    }
    session->Pause();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackSeek(const std::string& body) const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    const auto value = ExtractJsonNumberLiteral(body, "timestamp_ns");
    if (value.empty()) {
        if (JsonKeyExists(body, "timestamp_ns")) {
            return MakeErrorResponse(400, "timestamp_ns must be an unsigned integer");
        }
        return MakeErrorResponse(400, "timestamp_ns is required");
    }

    uint64_t timestampNs = 0;
    if (!TryParseUint64(value, &timestampNs)) {
        return MakeErrorResponse(400, "timestamp_ns must be an unsigned integer");
    }

    session->SeekTo(timestampNs);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackSpeed(const std::string& body) const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    const auto value = ExtractJsonNumberLiteral(body, "speed");
    if (value.empty()) {
        if (JsonKeyExists(body, "speed")) {
            return MakeErrorResponse(400, "speed must be numeric");
        }
        return MakeErrorResponse(400, "speed is required");
    }

    double speed = 0.0;
    if (!TryParseDouble(value, &speed)) {
        return MakeErrorResponse(400, "speed must be numeric");
    }

    session->SetSpeed(speed);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackLoop(const std::string& body) const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    const auto startValue = ExtractJsonNumberLiteral(body, "start_ns");
    const auto endValue = ExtractJsonNumberLiteral(body, "end_ns");
    if (startValue.empty() || endValue.empty()) {
        if ((startValue.empty() && JsonKeyExists(body, "start_ns")) ||
            (endValue.empty() && JsonKeyExists(body, "end_ns"))) {
            return MakeErrorResponse(400, "start_ns and end_ns must be unsigned integers");
        }
        return MakeErrorResponse(400, "start_ns and end_ns are required");
    }

    uint64_t startNs = 0;
    uint64_t endNs = 0;
    if (!TryParseUint64(startValue, &startNs) || !TryParseUint64(endValue, &endNs)) {
        return MakeErrorResponse(400, "start_ns and end_ns must be unsigned integers");
    }

    session->SetLoopRange(startNs, endNs);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackStop() const {
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session->Stop();
    BroadcastReplayTargetsIfAvailable();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlugins() const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }

#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    nlohmann::json response = nlohmann::json::array();
    for (const auto& plugin : runtimeCatalog->ListPlugins()) {
        nlohmann::json pluginJson = {
            {"id", plugin.id},
            {"name", plugin.name},
            {"version", plugin.version},
            {"state", plugin.state},
            {"kind", plugin.kind},
            {"protocol", plugin.protocol},
            {"desc", plugin.desc},
            {"bundle_path", plugin.bundle_path},
            {"config_fields", nlohmann::json::array()},
        };
        for (const auto& field : plugin.config_fields) {
            pluginJson["config_fields"].push_back({
                {"key", field.key},
                {"label", field.label},
                {"value", field.value},
            });
        }
        response.push_back(std::move(pluginJson));
    }
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandlePluginDetail(const std::string& pluginId) const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }

#if !RECPLAY_HTTP_SERVER_HAS_JSON
    (void)pluginId;
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    const auto plugin = runtimeCatalog->GetPlugin(pluginId);
    if (!plugin.has_value()) {
        return MakeErrorResponse(404, "Plugin not found");
    }

    nlohmann::json response = {
        {"id", plugin->id},
        {"name", plugin->name},
        {"version", plugin->version},
        {"state", plugin->state},
        {"kind", plugin->kind},
        {"protocol", plugin->protocol},
        {"desc", plugin->desc},
        {"bundle_path", plugin->bundle_path},
        {"config_fields", nlohmann::json::array()},
    };
    for (const auto& field : plugin->config_fields) {
        response["config_fields"].push_back({
            {"key", field.key},
            {"label", field.label},
            {"value", field.value},
        });
    }
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandlePluginConfig(const std::string& pluginId, const std::string& body) const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }

#if !RECPLAY_HTTP_SERVER_HAS_JSON
    (void)pluginId;
    (void)body;
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    try {
        const auto document = nlohmann::json::parse(body.empty() ? "{}" : body);
        const auto fieldsJson = document.value("config_fields", nlohmann::json::array());
        std::vector<PluginConfigFieldInfo> fields;
        for (const auto& item : fieldsJson) {
            fields.push_back(PluginConfigFieldInfo{
                item.value("key", std::string{}),
                item.value("label", std::string{}),
                item.value("value", std::string{}),
            });
        }

        if (!runtimeCatalog->SavePluginConfig(pluginId, fields)) {
            return MakeErrorResponse(404, "Plugin not found");
        }
        return HandlePluginDetail(pluginId);
    } catch (...) {
        return MakeErrorResponse(400, "Invalid plugin config payload");
    }
#endif
}

HttpResponse HttpServer::HandlePluginStart(const std::string& pluginId) const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }
    if (!runtimeCatalog->StartPlugin(pluginId)) {
        return MakeErrorResponse(400, "Failed to start plugin");
    }
    return HandlePluginDetail(pluginId);
}

HttpResponse HttpServer::HandlePluginStop(const std::string& pluginId) const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }
    if (!runtimeCatalog->StopPlugin(pluginId)) {
        return MakeErrorResponse(400, "Failed to stop plugin");
    }
    return HandlePluginDetail(pluginId);
}

HttpResponse HttpServer::HandleChannels() const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }

#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    const auto* stats = stats_.load(std::memory_order_acquire);
    const auto snapshot = stats != nullptr ? stats->GetSnapshot() : StatsSnapshot{};
    nlohmann::json response = nlohmann::json::array();
    for (const auto& channel : runtimeCatalog->ListChannels()) {
        const auto statsIt = snapshot.per_channel.find(channel.numeric_id);
        const auto* channelStats = statsIt != snapshot.per_channel.end() ? &statsIt->second : nullptr;
        response.push_back({
            {"id", channel.id},
            {"name", channel.name},
            {"topic", channel.topic},
            {"direction", channel.direction},
            {"protocol", channel.protocol},
            {"plugin_id", channel.plugin_id},
            {"rate", channelStats != nullptr ? nlohmann::json(channelStats->throughput_mbps) : nlohmann::json(nullptr)},
            {"packets", channelStats != nullptr ? nlohmann::json(channelStats->packets) : nlohmann::json(nullptr)},
        });
    }
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandleFiles() const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    namespace fs = std::filesystem;
    nlohmann::json files = nlohmann::json::array();
    auto* storage = storage_.load(std::memory_order_acquire);

    std::error_code ec;
    const fs::path dir(recordings_dir_);
    if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
        std::vector<fs::path> paths;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc) || fileEc) {
                continue;
            }
            if (it->path().extension() == ".rpcap") {
                paths.push_back(it->path());
            }
        }
        // Newest-first heuristic: capture filenames are timestamp-prefixed, so a
        // descending name sort approximates recency without reading mtimes.
        std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
            return a.filename().string() > b.filename().string();
        });

        for (const auto& path : paths) {
            std::error_code sizeEc;
            const auto size = fs::file_size(path, sizeEc);
            nlohmann::json item;
            item["name"] = path.filename().string();
            item["path"] = path.string();
            item["size"] = sizeEc ? nlohmann::json(nullptr)
                                  : nlohmann::json(static_cast<uint64_t>(size));

            // Metadata is read through the storage service interface (which owns
            // the .rpcap format) so the webserver bundle never links the storage
            // implementation. ProbeFile uses a transient reader, so this is safe
            // even while a file is open for playback.
            std::optional<RecordingFileInfo> info;
            if (storage != nullptr) {
                info = storage->ProbeFile(path.string());
            }
            if (info) {
                item["duration_ns"] = info->duration_ns;
                item["channels"] = info->channel_count;
                item["recorded_at"] = info->creation_time;
            } else {
                item["duration_ns"] = nullptr;
                item["channels"] = nullptr;
                item["recorded_at"] = nullptr;
            }
            files.push_back(std::move(item));
        }
    }

    nlohmann::json response = {{"files", files}};
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandleMarkers() const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    nlohmann::json markers = nlohmann::json::array();

    // Seed markers from the open playback file's keyframe (chunk-boundary)
    // timestamps. Returns empty when nothing is open for reading.
    // CODEX (阶段 8R): replace with real bookmarks / semantic events once a
    // marker store exists; this is a stand-in so the markers panel has content.
    auto* storage = storage_.load(std::memory_order_acquire);
    if (storage != nullptr && storage->IsReading()) {
        constexpr std::size_t kMaxMarkers = 500;
        const auto keyframes = storage->GetKeyframeTimestamps();
        std::size_t index = 0;
        for (const auto timestamp : keyframes) {
            if (index >= kMaxMarkers) {
                break;
            }
            markers.push_back({
                {"timestamp_ns", timestamp},
                {"label", "Keyframe " + std::to_string(index + 1)},
                {"type", "event"},
            });
            ++index;
        }
    }

    nlohmann::json response = {{"markers", markers}};
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandlePlaybackPacket() const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    const auto snapshot = session->GetCurrentPlaybackPacket();
    if (!snapshot.available) {
        return MakeJsonResponse("null");
    }

    const auto& packet = snapshot.packet;
    std::string protocol = "UNKNOWN";
    switch (static_cast<ProtocolId>(packet.protocol_id)) {
        case ProtocolId::kUDP:
            protocol = "UDP";
            break;
        case ProtocolId::kTCP:
            protocol = "TCP";
            break;
        case ProtocolId::kDDS:
            protocol = "DDS";
            break;
        case ProtocolId::kHLA:
            protocol = "HLA";
            break;
        case ProtocolId::kSerial:
            protocol = "Serial";
            break;
        default:
            break;
    }
    nlohmann::json response = {
        {"channel", packet.channel_id},
        {"plugin", protocol},
        {"protocol", protocol},
        {"seq", packet.sequence},
        {"t_record", packet.t_record},
        {"t_replay", snapshot.replay_timestamp_ns},
        {"size", packet.payload.size()},
        {"topic", packet.topic},
        {"writer", snapshot.writer},
        {"hex", BytesToHex(packet.payload)},
    };
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandlePlaybackDensity() const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    auto* storage = storage_.load(std::memory_order_acquire);
    if (storage == nullptr) {
        return MakeErrorResponse(503, "Storage service unavailable");
    }

    constexpr uint32_t kDensityBuckets = 256;
    const auto total = storage->GetDensity(kDensityBuckets);
    const auto header = storage->GetHeader();

    nlohmann::json totalJson = nlohmann::json::array();
    for (const auto value : total) {
        totalJson.push_back(value);
    }

    nlohmann::json response = {
        {"duration_ns", header.duration_ns},
        {"buckets", total.size()},
        {"total", std::move(totalJson)},
    };
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandleReplayTargets() const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    nlohmann::json targets = nlohmann::json::array();
    for (const auto& target : session->GetReplayTargets()) {
        targets.push_back({
            {"id", target.id},
            {"name", target.name},
            {"protocol", target.protocol},
            {"enabled", target.enabled},
            {"endpoint", BuildReplayTargetEndpoint(target.config)},
            {"status", target.status.empty() ? "idle" : target.status},
        });
    }

    nlohmann::json response = {{"targets", std::move(targets)}};
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandleSetReplayTargets(const std::string& body) const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    (void)body;
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    auto* session = session_.load(std::memory_order_acquire);
    if (session == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    try {
        const auto document = nlohmann::json::parse(body.empty() ? "{}" : body);
        const auto targetsJson = document.value("targets", nlohmann::json::array());
        std::vector<ISessionService::ReplayTarget> targets;
        for (const auto& item : targetsJson) {
            ISessionService::ReplayTarget target;
            target.id = item.value("id", std::string{});
            target.name = item.value("name", std::string{});
            target.protocol = item.value("protocol", std::string{});
            target.enabled = item.value("enabled", true);
            if (item.contains("endpoint") && item.at("endpoint").is_string()) {
                const auto endpoint = item.at("endpoint").get<std::string>();
                target.config["endpoint"] = endpoint;
                const auto colonPos = endpoint.rfind(':');
                if (colonPos != std::string::npos) {
                    const auto host = endpoint.substr(0, colonPos);
                    const auto port = endpoint.substr(colonPos + 1);
                    if (target.protocol == "UDP") {
                        target.config["address"] = host;
                        target.config["port"] = port;
                        target.config["bind_interface"] = "0.0.0.0";
                    } else if (target.protocol == "TCP") {
                        target.config["host"] = host;
                        target.config["port"] = port;
                        target.config["mode"] = "client";
                    }
                }
            }
            targets.push_back(std::move(target));
        }

        session->SetReplayTargets(targets);
        BroadcastReplayTargetsIfAvailable();
        return HandleReplayTargets();
    } catch (...) {
        return MakeErrorResponse(400, "Invalid replay targets payload");
    }
#endif
}

HttpResponse HttpServer::HandleLogs() const {
#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    nlohmann::json entries = nlohmann::json::array();
    if (auto* webSocketServer = websocket_server_.load(std::memory_order_acquire);
        webSocketServer != nullptr) {
        for (const auto& entry : webSocketServer->GetEventLogEntries()) {
            entries.push_back({
                {"ts", entry.timestamp_ms},
                {"level", entry.level},
                {"message", entry.message},
                {"source", entry.source},
            });
        }
    }
    nlohmann::json response = {{"entries", std::move(entries)}};
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandleMappings() const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }

#if !RECPLAY_HTTP_SERVER_HAS_JSON
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    nlohmann::json response = nlohmann::json::array();
    for (const auto& mapping : runtimeCatalog->GetTopicMappings()) {
        response.push_back({
            {"source_topic", mapping.source_topic},
            {"target_topic", mapping.target_topic},
        });
    }
    return MakeJsonResponse(response.dump());
#endif
}

HttpResponse HttpServer::HandleSetMappings(const std::string& body) const {
    auto* runtimeCatalog = runtime_catalog_.load(std::memory_order_acquire);
    if (runtimeCatalog == nullptr) {
        return MakeErrorResponse(503, "Runtime catalog unavailable");
    }

#if !RECPLAY_HTTP_SERVER_HAS_JSON
    (void)body;
    return MakeErrorResponse(500, "JSON support unavailable");
#else
    try {
        const auto document = nlohmann::json::parse(body.empty() ? "{}" : body);
        const auto mappingsJson = document.value("mappings", nlohmann::json::array());
        std::vector<RuntimeTopicMapping> mappings;
        for (const auto& item : mappingsJson) {
            mappings.push_back(RuntimeTopicMapping{
                item.value("source_topic", std::string{}),
                item.value("target_topic", std::string{}),
            });
        }

        if (!runtimeCatalog->SetTopicMappings(mappings)) {
            return MakeErrorResponse(400, "Failed to save topic mappings");
        }
        return MakeJsonResponse("{\"ok\":true}");
    } catch (...) {
        return MakeErrorResponse(400, "Invalid topic mapping payload");
    }
#endif
}

void HttpServer::BroadcastReplayTargetsIfAvailable() const {
    if (auto* webSocketServer = websocket_server_.load(std::memory_order_acquire);
        webSocketServer != nullptr) {
        webSocketServer->BroadcastReplayTargets();
    }
}

HttpResponse HttpServer::MakeJsonResponse(std::string body, int statusCode) {
    HttpResponse response;
    response.status_code = statusCode;
    response.content_type = "application/json";
    response.body = std::move(body);
    return response;
}

HttpResponse HttpServer::MakeErrorResponse(int statusCode, const std::string& message) {
    return MakeJsonResponse(
        "{\"error\":\"" + EscapeJsonString(message) + "\"}",
        statusCode);
}

std::string HttpServer::EscapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

std::string HttpServer::ExtractJsonNumberLiteral(const std::string& body, const std::string& key) {
    size_t start = 0;
    if (!FindTopLevelKeyValueStart(body, key, &start)) {
        return {};
    }

    if (start >= body.size()) {
        return {};
    }

    size_t end = start;
    if (body[end] == '-') {
        ++end;
    }

    bool sawDigits = false;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end])) != 0) {
        sawDigits = true;
        ++end;
    }

    if (end < body.size() && body[end] == '.') {
        ++end;
        while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end])) != 0) {
            sawDigits = true;
            ++end;
        }
    }

    if (!sawDigits) {
        return {};
    }

    if (end < body.size() && (body[end] == 'e' || body[end] == 'E')) {
        size_t exponentPos = end + 1;
        if (exponentPos < body.size() && (body[exponentPos] == '+' || body[exponentPos] == '-')) {
            ++exponentPos;
        }

        size_t exponentDigits = exponentPos;
        while (exponentDigits < body.size() &&
               std::isdigit(static_cast<unsigned char>(body[exponentDigits])) != 0) {
            ++exponentDigits;
        }

        if (exponentDigits == exponentPos) {
            return {};
        }
        end = exponentDigits;
    }

    return body.substr(start, end - start);
}

std::string HttpServer::ExtractJsonStringLiteral(const std::string& body, const std::string& key) {
    size_t start = 0;
    if (!FindTopLevelKeyValueStart(body, key, &start)) {
        return {};
    }
    if (start >= body.size() || body[start] != '"') {
        return {};
    }

    size_t cursor = start;
    if (!SkipJsonString(body, &cursor)) {
        return {};
    }
    std::string unescaped;
    if (!UnescapeJsonStringLiteral(body.substr(start + 1, cursor - start - 2), &unescaped)) {
        return {};
    }
    return unescaped;
}

std::string HttpServer::ExtractJsonObjectLiteral(const std::string& body, const std::string& key) {
    size_t start = 0;
    if (!FindTopLevelKeyValueStart(body, key, &start)) {
        return {};
    }
    if (start >= body.size() || (body[start] != '{' && body[start] != '[')) {
        return {};
    }

    const char open = body[start];
    const char close = (open == '{') ? '}' : ']';
    int depth = 0;
    size_t cursor = start;
    while (cursor < body.size()) {
        const char c = body[cursor];
        if (c == '"') {
            if (!SkipJsonString(body, &cursor)) {
                return {};
            }
            continue;
        }
        if (c == open) {
            ++depth;
        } else if (c == close) {
            --depth;
            if (depth == 0) {
                return body.substr(start, cursor - start + 1);
            }
        }
        ++cursor;
    }
    return {};
}

} // namespace recplay
