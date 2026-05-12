// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"

#if __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#define RECPLAY_HAS_DROGON 1
#else
#define RECPLAY_HAS_DROGON 0
#endif

namespace recplay {

bool HttpServer::Start(const std::string& host, int port) {
#if RECPLAY_HAS_DROGON
    drogon::app().addListener(host, static_cast<uint16_t>(port));
    drogon::app().registerHandler(
        "/api/health",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            response->setBody("{\"status\":\"ok\"}");
            callback(response);
        });
#else
    (void)host;
    (void)port;
#endif
    running_ = true;
    return true;
}

void HttpServer::Stop() {
#if RECPLAY_HAS_DROGON
    drogon::app().quit();
#endif
    running_ = false;
}

bool HttpServer::IsRunning() const {
    return running_;
}

} // namespace recplay
