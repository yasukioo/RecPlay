// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

#include <string>

namespace recplay {

class HttpServer {
public:
    bool Start(const std::string& host, int port);
    void Stop();
    bool IsRunning() const;

private:
    bool running_ = false;
};

} // namespace recplay
