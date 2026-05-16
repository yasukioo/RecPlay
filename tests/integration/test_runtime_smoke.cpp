// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "HttpServer.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using recplay::HttpServer;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestHttpServerStartStopTwice() {
    HttpServer server(nullptr, nullptr);

    Expect(server.Start("127.0.0.1", 0), "first start should succeed");
    Expect(server.IsRunning(), "server should report running after first start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after first stop");

    Expect(server.Start("127.0.0.1", 0), "second start should succeed");
    Expect(server.IsRunning(), "server should report running after second start");
    server.Stop();
    Expect(!server.IsRunning(), "server should report stopped after second stop");
}

} // namespace

int main() {
    try {
        TestHttpServerStartStopTwice();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
