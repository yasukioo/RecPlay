// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#pragma once

namespace recplay {

class WebSocketServer {
public:
    bool Start();
    void Stop();
    bool IsRunning() const;

private:
    bool running_ = false;
};

} // namespace recplay
