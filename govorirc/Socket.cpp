#include "Socket.h"

#include <csignal>
#include <cstdio>

#include <openssl/err.h>
#include <openssl/ssl.h>

constexpr size_t kRecvChunk  = 4096;
constexpr size_t kMaxRecvBuf = 64 * 1024;
constexpr size_t kMaxSendBuf = 1024 * 1024;

static void initNet() 
{
#ifdef _WIN32
    static bool started = false;
    if (!started) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        started = true;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

static void logSslErrors(const char* what);

static SSL_CTX* makeClientCtx()
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        logSslErrors("SSL_CTX_new");
        return nullptr;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE |
                          SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    return ctx;
}

static SSL_CTX* clientCtx() 
{
    static SSL_CTX* ctx = makeClientCtx();
    return ctx;
}

static void logSslErrors(const char* what) 
{
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        std::fprintf(stderr, "%s: %s\n", what, buf);
    }
}

Socket::Socket(const std::string& host, const std::string& port, bool insecure)
    : insecure_(insecure)
{
    initNet();
    if (!clientCtx() || !resolveAndConnect(host, port))
        closed_ = true;
}

Socket::~Socket() 
{
    close();
}

bool Socket::resolveAndConnect(const std::string& host, const std::string& port) 
{
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0)
        return false;

    for (addrinfo* p = result; p; p = p->ai_next) {
        sock_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock_ == kInvalidSocket)
            continue;

        if (::connect(sock_, p->ai_addr, static_cast<socklen_type>(p->ai_addrlen)) != 0) {
            closeSock(sock_);
            sock_ = kInvalidSocket;
            continue;
        }

        setNonBlocking(sock_);

        ssl_ = SSL_new(clientCtx());
        if (!ssl_) {
            logSslErrors("SSL_new");
            closeSock(sock_);
            sock_ = kInvalidSocket;
            continue;
        }

        SSL_set_fd(ssl_, static_cast<int>(sock_));
        SSL_set_tlsext_host_name(ssl_, host.c_str());
        if (insecure_)
            SSL_set_verify(ssl_, SSL_VERIFY_NONE, nullptr);
        else
            SSL_set1_host(ssl_, host.c_str());
        SSL_set_connect_state(ssl_);

        if (!doHandshake()) {
            SSL_free(ssl_);
            ssl_ = nullptr;
            closeSock(sock_);
            sock_ = kInvalidSocket;
            continue;
        }

        ::freeaddrinfo(result);
        return true;
    }

    ::freeaddrinfo(result);
    std::fprintf(stderr, "failed to connect to %s:%s\n", host.c_str(), port.c_str());
    return false;
}

bool Socket::doHandshake() 
{
    for (;;) {
        int r = SSL_connect(ssl_);
        if (r == 1) {
            handshakeDone_ = true;
            return true;
        }

        int err = SSL_get_error(ssl_, r);
        pollfd_t pfd{};
        pfd.fd = sock_;
        if (err == SSL_ERROR_WANT_READ) {
            pfd.events = POLLIN;
        }
        else if (err == SSL_ERROR_WANT_WRITE) {
            pfd.events = POLLOUT;
        }
        else {
            logSslErrors("SSL_connect");
            return false;
        }

        int n = pollSockets(&pfd, 1, 10000);
        if (n < 0) {
            if (isInterrupted(lastSockError()))
                continue;
            std::fprintf(stderr, "poll: %d\n", lastSockError());
            return false;
        }
        if (n == 0) {
            std::fprintf(stderr, "TLS handshake timeout\n");
            return false;
        }
    }
}

bool Socket::poll(int timeoutMs)
{
    if (closed_)
        return false;

    pollfd_t pfd{};
    pfd.fd = sock_;
    pfd.events = 0;
    if (wantRead_)
        pfd.events |= POLLIN;
    if (wantWrite_ || !sendBuf_.empty())
        pfd.events |= POLLOUT;

    int n = pollSockets(&pfd, 1, timeoutMs);
    if (n < 0) {
        if (isInterrupted(lastSockError()))
            return !closed_;
        close();
        return false;
    }
    if (n == 0)
        return true;

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        close();
        return false;
    }

    if (pfd.revents & POLLIN)
        onReadable();
    if (!closed_ && (pfd.revents & POLLOUT))
        onWritable();

    return !closed_;
}

void Socket::onReadable() 
{
    char buf[kRecvChunk]{};
    for (;;) {
        int n = SSL_read(ssl_, buf, sizeof(buf));
        if (n > 0) {
            if (recvBuf_.size() + static_cast<std::size_t>(n) > kMaxRecvBuf) {
                std::fprintf(stderr, "recv buffer overflow\n");
                close();
                return;
            }
            recvBuf_.append(buf, static_cast<std::size_t>(n));
            continue;
        }

        int err = SSL_get_error(ssl_, n);
        if (err == SSL_ERROR_WANT_READ) {
            wantRead_ = true;
            wantWrite_ = false;
            break;
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            wantRead_ = false;
            wantWrite_ = true;
            break;
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            close();
            return;
        }
        logSslErrors("SSL_read");
        close();
        return;
    }
    processBuffer();
}

void Socket::onWritable() 
{
    while (!sendBuf_.empty()) {
        int n = SSL_write(ssl_, sendBuf_.data(), static_cast<int>(sendBuf_.size()));
        if (n > 0) {
            sendBuf_.erase(0, static_cast<std::size_t>(n));
            continue;
        }

        int err = SSL_get_error(ssl_, n);
        if (err == SSL_ERROR_WANT_READ) {
            wantRead_ = true;
            return;
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            wantWrite_ = true;
            return;
        }
        logSslErrors("SSL_write");
        close();
        return;
    }
    wantWrite_ = false;
}

void Socket::processBuffer() 
{
    for (;;) {
        std::size_t pos = recvBuf_.find('\n');
        if (pos == std::string::npos)
            return;

        std::string_view line(recvBuf_.data(), pos);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (!line.empty() && lineHandler_)
            lineHandler_(line);

        recvBuf_.erase(0, pos + 1);
        if (closed_)
            return;
    }
}

bool Socket::send(std::string_view data) 
{
    if (closed_)
        return false;
    if (sendBuf_.size() + data.size() > kMaxSendBuf) {
        std::fprintf(stderr, "send buffer overflow\n");
        close();
        return false;
    }
    sendBuf_.append(data);
    return true;
}

void Socket::close() 
{
    if (closed_)
        return;
    closed_ = true;

    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (sock_ != kInvalidSocket) {
        closeSock(sock_);
        sock_ = kInvalidSocket;
    }
}