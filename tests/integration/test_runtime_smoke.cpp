// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"

#if __has_include(<drogon/HttpClient.h>)
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#define RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT 1
#else
#define RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT 0
#endif

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using recplay::HttpServer;
constexpr int kHttpServerPort = 18081;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Predicate>
bool WaitUntil(Predicate&& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

bool RequestHealth(int port) {
#if RECPLAY_TEST_HAS_DROGON_HTTP_CLIENT
    std::atomic<bool> done{false};
    std::atomic<bool> healthy{false};

    auto client =
        drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(port));
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath("/api/health");
    client->sendRequest(
        request,
        [&](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
            const bool ok =
                result == drogon::ReqResult::Ok &&
                response != nullptr &&
                response->statusCode() == drogon::k200OK &&
                response->getBody() == "{\"status\":\"ok\"}";
            healthy.store(ok, std::memory_order_release);
            done.store(true, std::memory_order_release);
        });

    if (!WaitUntil([&] {
            return done.load(std::memory_order_acquire);
        }, std::chrono::milliseconds(1000))) {
        return false;
    }

    return healthy.load(std::memory_order_acquire);
#else
    (void)port;
    return true;
#endif
}

void TestHttpServerStartStopTwice() {
    HttpServer server(nullptr, nullptr);

    Expect(server.Start("127.0.0.1", kHttpServerPort), "first start should succeed");
    Expect(server.IsRunning(), "server should report running after first start");
    Expect(RequestHealth(kHttpServerPort), "health endpoint should respond after first start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after first stop");

    Expect(server.Start("127.0.0.1", kHttpServerPort), "second start should succeed");
    Expect(server.IsRunning(), "server should report running after second start");
    Expect(RequestHealth(kHttpServerPort), "health endpoint should respond after second start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after second stop");
}

void TestHttpServerRestartWithNewInstance() {
    {
        HttpServer firstServer(nullptr, nullptr);
        Expect(firstServer.Start("127.0.0.1", kHttpServerPort), "first instance should start");
        Expect(RequestHealth(kHttpServerPort), "health endpoint should respond for first instance");
        firstServer.Stop();
        Expect(!firstServer.IsRunning(), "first instance should stop cleanly");
    }

    HttpServer secondServer(nullptr, nullptr);
    Expect(secondServer.Start("127.0.0.1", kHttpServerPort), "second instance should start on same port");
    Expect(RequestHealth(kHttpServerPort), "health endpoint should respond for second instance");
    secondServer.Stop();
    Expect(!secondServer.IsRunning(), "second instance should stop cleanly");
}

} // namespace

int main() {
    try {
        TestHttpServerStartStopTwice();
        TestHttpServerRestartWithNewInstance();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
