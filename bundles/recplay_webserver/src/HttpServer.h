// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include "StaticFileHandler.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace recplay {

class ISessionService;
class IStatsService;
class IStorageService;
class IRuntimeCatalog;
class WebSocketServer;

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::string body;
};

class HttpServer {
public:
    HttpServer(ISessionService* session = nullptr,
               IStatsService* stats = nullptr,
               IRuntimeCatalog* runtimeCatalog = nullptr);

    bool Start(const std::string& host, int port);
    void Stop();
    bool IsRunning() const;
    void SetSessionService(ISessionService* session);
    void SetStatsService(IStatsService* stats);
    void SetRuntimeCatalog(IRuntimeCatalog* runtimeCatalog);
    void SetStorageService(IStorageService* storage);
    void SetWebSocketServer(WebSocketServer* webSocketServer);
    bool SetStaticRoot(const std::string& root);
    // Directory scanned by GET /api/files for *.rpcap recordings. Set once at
    // startup (before Start); defaults to the configured storage output dir.
    void SetRecordingsDirectory(const std::string& directory);
    HttpResponse HandleRequest(const std::string& method,
                               const std::string& path,
                               const std::string& body) const;

private:
    HttpResponse HandleSessionState() const;
    HttpResponse HandleStats() const;
    HttpResponse HandleSessionReset() const;
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
    HttpResponse HandlePlugins() const;
    HttpResponse HandlePluginDetail(const std::string& pluginId) const;
    HttpResponse HandlePluginConfig(const std::string& pluginId, const std::string& body) const;
    HttpResponse HandlePluginStart(const std::string& pluginId) const;
    HttpResponse HandlePluginStop(const std::string& pluginId) const;
    HttpResponse HandleChannels() const;
    HttpResponse HandleFiles() const;
    HttpResponse HandleMarkers() const;
    HttpResponse HandlePlaybackPacket() const;
    HttpResponse HandlePlaybackDensity() const;
    HttpResponse HandleReplayTargets() const;
    HttpResponse HandleSetReplayTargets(const std::string& body) const;
    HttpResponse HandleLogs() const;
    HttpResponse HandleMappings() const;
    HttpResponse HandleSetMappings(const std::string& body) const;
    void BroadcastReplayTargetsIfAvailable() const;
    static HttpResponse MakeJsonResponse(std::string body, int statusCode = 200);
    static HttpResponse MakeErrorResponse(int statusCode, const std::string& message);
    static std::string EscapeJsonString(const std::string& value);
    static std::string ExtractJsonNumberLiteral(const std::string& body, const std::string& key);
    static std::string ExtractJsonStringLiteral(const std::string& body, const std::string& key);
    static std::string ExtractJsonObjectLiteral(const std::string& body, const std::string& key);

    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::atomic<ISessionService*> session_{nullptr};
    std::atomic<IStatsService*> stats_{nullptr};
    std::atomic<IRuntimeCatalog*> runtime_catalog_{nullptr};
    std::atomic<IStorageService*> storage_{nullptr};
    std::atomic<WebSocketServer*> websocket_server_{nullptr};
    StaticFileHandler static_files_;
    std::string recordings_dir_{"data"};
};

} // namespace recplay
