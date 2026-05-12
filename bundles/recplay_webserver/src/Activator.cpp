// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"
#include "WebSocketServer.h"

#include <cppmicroservices/BundleActivator.h>

#include <memory>

namespace recplay {

class WebserverActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        (void)context;
        http_server_ = std::make_unique<HttpServer>();
        websocket_server_ = std::make_unique<WebSocketServer>();
        http_server_->Start("0.0.0.0", 8080);
        websocket_server_->Start();
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        if (websocket_server_) {
            websocket_server_->Stop();
        }
        if (http_server_) {
            http_server_->Stop();
        }
        websocket_server_.reset();
        http_server_.reset();
    }

private:
    std::unique_ptr<HttpServer> http_server_;
    std::unique_ptr<WebSocketServer> websocket_server_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::WebserverActivator)
