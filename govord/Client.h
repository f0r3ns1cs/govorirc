#pragma once

#include <string>
#include <set>
#include <vector>
#include <chrono>

#include "SocketCompat.h"

#include <openssl/types.h>

struct Channel;

struct Client
{
    int       id = -1;
    socket_t  sock = kInvalidSocket;
    SSL* ssl = nullptr;

    std::string host;
    std::string nick;
    std::string user;
    std::string realname;

    bool registered = false;
    bool capNegotiating = false;
    bool cap302 = false;
    std::set<std::string> enabledCaps;

    std::string recvBuf;
    std::string sendBuf;

    bool handshakeDone = false;
    bool wantRead = true;
    bool wantWrite = false;
    bool markedForClose = false;
    std::string closeReason;

    std::chrono::steady_clock::time_point lastInviteAt{};
    std::chrono::steady_clock::time_point connectedAt;
    std::chrono::steady_clock::time_point lastActiveAt;
    std::chrono::steady_clock::time_point pingSentAt;
    bool pingPending = false;

    std::string suppliedPass;
    std::string awayMessage;
    bool isOper = false;

    std::vector<Channel*> channels;
};