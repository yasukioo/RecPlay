// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace recplay {

class ISessionService;
class IStatsService;

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::string body;
};

class HttpServer {
public:
    HttpServer(ISessionService* session = nullptr, IStatsService* stats = nullptr);

    bool Start(const std::string& host, int port);
    void Stop();
    bool IsRunning() const;
    void SetSessionService(ISessionService* session);
    void SetStatsService(IStatsService* stats);
    HttpResponse HandleRequest(const std::string& method,
                               const std::string& path,
                               const std::string& body) const;

private:
    HttpResponse HandleSessionState() const;
    HttpResponse HandleStats() const;
    HttpResponse HandleRecordStart(const std::string& body) const;
    HttpResponse HandleRecordPause() const;
    HttpResponse HandleRecordResume() const;
    HttpResponse HandleRecordStop() const;
    HttpResponse HandlePlaybackOpen(const std::string& body) const;
    HttpResponse HandlePlaybackPlay(const std::string& body) const;
    HttpResponse HandlePlaybackPause() const;
    HttpResponse HandlePlaybackSeek(const std::string& body) const;
    HttpResponse HandlePlaybackSpeed(const std::string& body) const;
    HttpResponse HandlePlaybackLoop(const std::string& body) const;
    HttpResponse HandlePlaybackStop() const;
    static HttpResponse MakeJsonResponse(std::string body, int statusCode = 200);
    static HttpResponse MakeErrorResponse(int statusCode, const std::string& message);
    static std::string EscapeJsonString(const std::string& value);
    static std::string ExtractJsonNumberLiteral(const std::string& body, const std::string& key);
    static std::string ExtractJsonStringLiteral(const std::string& body, const std::string& key);

    std::atomic<bool> running_{false};
    bool drogon_configured_ = false;
    std::thread server_thread_;
    ISessionService* session_ = nullptr;
    IStatsService* stats_ = nullptr;
};

} // namespace recplay
