#include "SocketCompat.h"

#ifdef _WIN32
#include <cstdio>
#else
#include <netinet/tcp.h>
#include <system_error>
#endif

int lastSockError() noexcept 
{
#ifdef _WIN32
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

bool isWouldBlock(int err) noexcept 
{
#ifdef _WIN32
    return err == WSAEWOULDBLOCK;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

bool isInterrupted(int err) noexcept 
{
#ifdef _WIN32
    (void)err;
    return false;
#else
    return err == EINTR;
#endif
}

bool isConnReset(int err) noexcept 
{
#ifdef _WIN32
    return err == WSAECONNRESET || err == WSAECONNABORTED;
#else
    return err == ECONNRESET || err == ECONNABORTED || err == EPIPE;
#endif
}

std::string sockErrorString(int err) 
{
#ifdef _WIN32
    char* buf = nullptr;
    const DWORD len = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(err),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buf), 0, nullptr);
    if (len == 0 || buf == nullptr) return "error " + std::to_string(err);

    std::string msg(buf, len);
    ::LocalFree(buf);
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' '))
        msg.pop_back();

    return msg;
#else
    return std::system_category().message(err);
#endif
}

int closeSock(socket_t s) noexcept 
{
    if (s == kInvalidSocket) return 0;
#ifdef _WIN32
    return ::closesocket(s);
#else
    int rc;
    do {
        rc = ::close(s);
    } while (rc != 0 && errno == EINTR);
    return rc;
#endif
}

bool setNonBlocking(socket_t s) noexcept 
{
#ifdef _WIN32
    u_long mode = 1;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags == -1) return false;
    if (flags & O_NONBLOCK) return true;
    return ::fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

bool setReuseAddr(socket_t s, bool on) noexcept 
{
    const int yes = on ? 1 : 0;
    return ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&yes),
        sizeof(yes)) == 0;
}

bool setTcpNoDelay(socket_t s, bool on) noexcept 
{
    const int yes = on ? 1 : 0;
    return ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&yes),
        sizeof(yes)) == 0;
}

int pollSockets(pollfd_t* fds, std::size_t nfds, int timeoutMs) noexcept 
{
#ifdef _WIN32
    return ::WSAPoll(fds, static_cast<ULONG>(nfds), timeoutMs);
#else
    return ::poll(fds, static_cast<nfds_t>(nfds), timeoutMs);
#endif
}
