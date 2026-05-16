// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"

#include "IStatsService.h"
#include "ISessionService.h"

#include <cstdint>

#if defined(_WIN32)
#include <WinSock2.h>
#ifndef htonll
static inline std::uint64_t recplay_htonll(std::uint64_t value) {
    const std::uint32_t high = htonl(static_cast<std::uint32_t>(value >> 32));
    const std::uint32_t low = htonl(static_cast<std::uint32_t>(value & 0xffffffffULL));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}
#define htonll recplay_htonll
#endif
#endif

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define RECPLAY_HAS_DROGON 1
#else
#define RECPLAY_HAS_DROGON 0
#endif

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <sstream>

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
#endif

} // namespace

HttpServer::HttpServer(ISessionService* session, IStatsService* stats)
    : session_(session), stats_(stats) {}

bool HttpServer::Start(const std::string& host, int port) {
    if (running_.exchange(true)) {
        return true;
    }

#if RECPLAY_HAS_DROGON
    if (!drogon_configured_) {
        drogon::app().addListener(host, static_cast<uint16_t>(port));

        const auto registerRoute =
            [this](const std::string& method, const std::string& path) {
                drogon::app().registerHandler(
                    path,
                    [this, method, path](const drogon::HttpRequestPtr& request,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                        const auto response =
                            HandleRequest(method, path, std::string(request->getBody()));
                        auto httpResponse = drogon::HttpResponse::newHttpResponse();
                        httpResponse->setStatusCode(ToDrogonStatus(response.status_code));
                        httpResponse->setContentTypeString(response.content_type);
                        httpResponse->setBody(response.body);
                        callback(httpResponse);
                    },
                    {ToDrogonMethod(method)});
            };

        drogon::app().registerHandler(
            "/api/health",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                response->setBody("{\"status\":\"ok\"}");
                callback(response);
            });

        registerRoute("GET", "/api/session/state");
        registerRoute("GET", "/api/stats");
        registerRoute("POST", "/api/session/record");
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
        drogon_configured_ = true;
    }

    server_thread_ = std::thread([] {
        try {
            drogon::app().run();
        } catch (...) {
        }
    });
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
    drogon::app().quit();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
#endif
}

bool HttpServer::IsRunning() const {
    return running_;
}

void HttpServer::SetSessionService(ISessionService* session) {
    session_ = session;
}

void HttpServer::SetStatsService(IStatsService* stats) {
    stats_ = stats;
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
    return MakeErrorResponse(404, "Not Found");
}

HttpResponse HttpServer::HandleSessionState() const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    std::ostringstream stream;
    stream << "{\"state\":\"" << SessionStateToString(session_->GetState()) << "\""
           << ",\"duration_ns\":" << session_->GetDuration()
           << ",\"position_ns\":" << session_->GetCurrentPosition()
           << ",\"speed\":" << session_->GetCurrentSpeed()
           << "}";
    return MakeJsonResponse(stream.str());
}

HttpResponse HttpServer::HandleStats() const {
    if (stats_ == nullptr) {
        return MakeErrorResponse(503, "Stats service unavailable");
    }

    const auto snapshot = stats_->GetSnapshot();
    std::ostringstream stream;
    stream << "{\"total_throughput_mbps\":" << snapshot.total_throughput_mbps
           << ",\"total_packets\":" << snapshot.total_packets
           << ",\"total_drops\":" << snapshot.total_drops
           << ",\"drop_rate\":" << snapshot.drop_rate
           << ",\"write_latency_p99_ms\":" << snapshot.write_latency_p99_ms
           << ",\"ringbuf_used\":" << snapshot.ringbuf_used
           << ",\"ringbuf_capacity\":" << snapshot.ringbuf_capacity
           << ",\"disk_queue_bytes\":" << snapshot.disk_queue_bytes
           << "}";
    return MakeJsonResponse(stream.str());
}

HttpResponse HttpServer::HandleRecordStart(const std::string& body) const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    if (!session_->StartRecording(body)) {
        return MakeErrorResponse(400, "Failed to start recording");
    }
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleRecordPause() const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session_->PauseRecording();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleRecordResume() const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session_->ResumeRecording();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandleRecordStop() const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session_->StopRecording();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackOpen(const std::string& body) const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }

    const auto filePath = ExtractJsonStringLiteral(body, "file_path");
    if (filePath.empty()) {
        return MakeErrorResponse(400, "file_path is required");
    }
    if (!session_->OpenForPlayback(filePath)) {
        return MakeErrorResponse(400, "Failed to open playback file: " + filePath);
    }
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackPlay(const std::string& body) const {
    if (session_ == nullptr) {
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
    session_->Play(speed);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackPause() const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    if (session_->GetState() != SessionState::Playing) {
        return MakeErrorResponse(409, "Playback is not currently playing");
    }
    session_->Pause();
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackSeek(const std::string& body) const {
    if (session_ == nullptr) {
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

    session_->SeekTo(timestampNs);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackSpeed(const std::string& body) const {
    if (session_ == nullptr) {
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

    session_->SetSpeed(speed);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackLoop(const std::string& body) const {
    if (session_ == nullptr) {
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

    session_->SetLoopRange(startNs, endNs);
    return MakeJsonResponse("{\"ok\":true}");
}

HttpResponse HttpServer::HandlePlaybackStop() const {
    if (session_ == nullptr) {
        return MakeErrorResponse(503, "Session service unavailable");
    }
    session_->Stop();
    return MakeJsonResponse("{\"ok\":true}");
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
    return body.substr(start + 1, cursor - start - 2);
}

} // namespace recplay
