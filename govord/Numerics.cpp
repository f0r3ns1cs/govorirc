#include "Numerics.h"
#include "config.h"

#include <format>

void Numerics::send(const Sink& sink, Client& c, std::string_view serverName,
    int code, std::string_view message) {
    std::string msg;
    msg.reserve(16 + serverName.size() + c.nick.size() + message.size());
    msg.append(":")
       .append(serverName)
       .append(" ")
       .append(std::format("{:03}", code))
       .append(" ")
       .append(c.nick.empty() ? "*" : c.nick)
       .append(" ")
       .append(message)
       .append("\r\n");
    sink(c, msg);
}

void Numerics::sendWelcome(const Sink& sink, Client& c,
    std::string_view serverName, std::string_view network) {
    const std::string mask = std::format("{}!~{}@{}", c.nick, c.user, c.host);

    sink(c, std::format(":{} 001 {} :Welcome to the {} IRC Network, {}\r\n",
        serverName, c.nick, network, mask));
    sink(c, std::format(":{} 002 {} :Your host is {}, running version {}\r\n",
        serverName, c.nick, serverName, SERVER_VERSION));
    sink(c, std::format(":{} 003 {} :This server was created {}\r\n",
        serverName, c.nick, SERVER_CREATED_AT));
    sink(c, std::format(":{} 004 {} {} {} {} {}\r\n",
        serverName, c.nick, serverName, SERVER_VERSION, "o", "imtklbov"));
}
