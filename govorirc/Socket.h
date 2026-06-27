#pragma once

#include "SocketCompat.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <openssl/types.h>

class Socket 
{
public:
    using LineHandler = std::function<void(std::string_view)>;

    // skips TLS cert verification so I can use self-signed certs (DO NOT USE THIS AGAINST AN UNTRUSTED NETWORK)
    Socket(const std::string& host, const std::string& port, bool insecure = false);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    void setLineHandler(LineHandler h) { lineHandler_ = std::move(h); }

    bool send(std::string_view data);
    bool poll(int timeoutMs);

    void close();
    bool closed() const noexcept { return closed_; }

private:
    bool resolveAndConnect(const std::string& host, const std::string& port);
    bool doHandshake();

    void onReadable();
    void onWritable();
    void processBuffer();

    socket_t        sock_ = kInvalidSocket;
    SSL*            ssl_ = nullptr;
    bool            handshakeDone_ = false;
    bool            closed_ = false;
    bool            insecure_ = false;

    bool            wantRead_ = true;
    bool            wantWrite_ = false;

    std::string     recvBuf_;
    std::string     sendBuf_;

    LineHandler     lineHandler_;
};