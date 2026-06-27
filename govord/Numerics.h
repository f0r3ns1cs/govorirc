#pragma once

#include <string>
#include <string_view>
#include <functional>

#include "Client.h"

namespace Numerics 
{
    using Sink = std::function<void(Client&, std::string_view)>;

    // formats ":<server> <code> <nick> <message>".
    void send(const Sink& sink, Client& c, std::string_view serverName, int code, std::string_view message);

    // 001-004 welcome burst.
    void sendWelcome(const Sink& sink, Client& c, std::string_view serverName, std::string_view network);
}
