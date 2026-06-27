#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

using socket_t = SOCKET;
using pollfd_t = WSAPOLLFD;
using socklen_type = int;

inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>

using socket_t = int;
using pollfd_t = ::pollfd;
using socklen_type = ::socklen_t;

inline constexpr socket_t kInvalidSocket = -1;
#endif

#include <string>

[[nodiscard]] int  lastSockError() noexcept;
[[nodiscard]] bool isWouldBlock(int err) noexcept;
[[nodiscard]] bool isInterrupted(int err) noexcept;
[[nodiscard]] bool isConnReset(int err) noexcept;
[[nodiscard]] std::string sockErrorString(int err);
int closeSock(socket_t s) noexcept;
[[nodiscard]] bool setNonBlocking(socket_t s) noexcept;
[[nodiscard]] bool setReuseAddr(socket_t s, bool on = true) noexcept;
[[nodiscard]] bool setTcpNoDelay(socket_t s, bool on = true) noexcept;
[[nodiscard]] int pollSockets(pollfd_t* fds, std::size_t nfds, int timeoutMs) noexcept;
