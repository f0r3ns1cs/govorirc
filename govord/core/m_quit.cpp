#include "m_quit.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

#include <algorithm>
#include <unordered_set>

void CommandQuit::execute(Server& s, Client& c, const Message& msg) const
{
    // QUIT [:<reason>]

    std::string reason = !msg.params.empty()
        ? Utils::stripCRLF(msg.params[0], 300)
        : std::string("Client Quit");

    if (!c.registered || c.nick.empty()) {
        s.closeClient(c.id, reason);
        return;
    }

    std::string line;
    line.append(":")
        .append(c.nick)
        .append("!")
        .append(c.user)
        .append("@")
        .append(c.host)
        .append(" QUIT :")
        .append(reason)
        .append("\r\n");

    std::unordered_set<int> recipients;
    for (Channel* ch : c.channels) {
        if (!ch) continue;
        for (int cid : ch->members) {
            if (cid == c.id) continue;
            recipients.insert(cid);
        }
    }

    for (int cid : recipients) {
        s.sendTo(cid, line);
    }

    s.closeClient(c.id, reason);
}
static AutoRegisterCommand<CommandQuit> s_reg_quit("QUIT");
