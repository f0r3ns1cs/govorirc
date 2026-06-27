#include "m_pingpong.h"
#include "IrcMessage.h"
#include "../Server.h"
#include <chrono>

void CommandPing::execute(Server& s, Client& c, const Message& msg) const
{
    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, ERR_NOORIGIN, ":No origin specified");
        return;
    }

    const std::string& token = msg.params[0];

    // Reply: ":<server> PONG <server> :<token>"
    std::string out = ":" + std::string(s.name()) +
        " PONG " + std::string(s.name()) +
        " :" + token + "\r\n";

    s.sendTo(c, out);
}

void CommandPong::execute(Server& s, Client& c, const Message& /*msg*/) const
{
    // A PONG with no token is harmless
    (void)s;

    c.lastActiveAt = std::chrono::steady_clock::now();
    c.pingPending = false;
}
static AutoRegisterCommand<CommandPing> s_reg_ping("PING");
static AutoRegisterCommand<CommandPong> s_reg_pong("PONG");
