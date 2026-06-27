#include "m_nick.h"
#include "IrcMessage.h"

#include "../Server.h"
#include "Utils.h"

#include <format>
#include <unordered_set>

void CommandNick::execute(Server& s, Client& c, const Message& msg) const
{
    if (msg.params.empty()) {
        s.sendTo(c, std::format(":{} 431 * :No nickname given\r\n", s.name()));
        return;
    }
    
    std::string requestedNick = std::string(msg.params[0]);
    if (!Utils::isValidNick(requestedNick)) {
        s.sendTo(c, std::format(":{} 432 {} {} :Erroneous nickname\r\n",
            s.name(), c.nick.empty() ? "*" : c.nick, requestedNick));
        return;
    }

    // alice -> Alice is a rename; alice -> alice is no-op.
    bool isSelfRename = !c.nick.empty() && Utils::sameNick(requestedNick, c.nick);
    if (isSelfRename && requestedNick == c.nick)
        return;

    if (!isSelfRename && s.nickExists(requestedNick)) {
        s.sendTo(c, std::format(":{} 433 {} {} :Nickname is already in use\r\n",
            s.name(), c.nick.empty() ? "*" : c.nick, requestedNick));
        return;
    }

    if (!c.nick.empty()) {
        s.recordWhowas(c);

        std::string oldNick = c.nick;
        std::string line = std::format(":{}!~{}@{} NICK :{}\r\n",
            oldNick, c.user, c.host, requestedNick);
        s.unregisterNick(oldNick);
        s.registerNick(requestedNick, c.id);
        c.nick = requestedNick;

        std::unordered_set<int> recipients{ c.id };
        for (Channel* ch : c.channels) {
            if (!ch) continue;
            for (int m : ch->members) recipients.insert(m);
        }
        for (int rid : recipients)
            if (Client* r = s.findClient(rid)) s.sendTo(*r, line);
        return;
    }
    s.registerNick(requestedNick, c.id);
    c.nick = requestedNick;
    s.tryCompleteRegistration(c);
}

static AutoRegisterCommand<CommandNick> s_reg_nick("NICK");
