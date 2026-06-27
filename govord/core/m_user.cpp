#include "m_user.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"
#include <format>

void CommandUser::execute(Server& s, Client& c, const Message& msg) const
{
    if (c.registered) {
        s.sendTo(c, std::format(":{} 462 {} :You may not reregister\r\n",
            s.name(), c.nick.empty() ? "*" : c.nick));
        return;
    }

    if (msg.params.size() < 4) {
        s.sendTo(c, std::format(":{} 461 {} USER :Not enough parameters\r\n",
            s.name(), c.nick.empty() ? "*" : c.nick));
        return;
    }

    // USER <username> <mode> <unused> :<realname>
    // mode and 'unused' get ignored
    std::string_view rawUser = msg.params[0];
    std::string_view rawRealname = msg.params[3];

    // server reserves leading '~'
    while (!rawUser.empty() && rawUser.front() == '~')
        rawUser.remove_prefix(1);

    if (rawUser.empty() || !Utils::isValidUsername(rawUser)) {
        s.sendTo(c, std::format(":{} 461 {} USER :Invalid username\r\n",
            s.name(), c.nick.empty() ? "*" : c.nick));
        return;
    }

    if (rawUser.size() > MAX_USERLEN)
        rawUser.remove_suffix(rawUser.size() - MAX_USERLEN);

    c.user = std::string(rawUser);

    std::string realname(rawRealname);
    for (char& ch : realname)
        if (ch == '\r' || ch == '\n' || ch == '\0') ch = ' ';
    if (realname.size() > MAX_REALNAMELEN)
        realname.resize(MAX_REALNAMELEN);
    c.realname = realname;

    s.tryCompleteRegistration(c);
}
static AutoRegisterCommand<CommandUser> s_reg_user("USER");
