#include "m_kick.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

void CommandKick::execute(Server& s, Client& c, const Message& msg) const
{
    // KICK <channel> <user> [:<comment>]
    if (msg.params.size() < 2 || msg.params[0].empty() || msg.params[1].empty()) {
        s.sendNumeric(c, 461, "KICK :Not enough parameters");
        return;
    }

    const std::string& chName = msg.params[0];
    const std::string& targetNick = msg.params[1];

    Channel* ch = s.getChannel(chName);
    if (!ch) {
        s.sendNumeric(c, 403, chName + " :No such channel");
        return;
    }

    if (!ch->members.count(c.id)) {
        s.sendNumeric(c, 442, chName + " :You're not on that channel");
        return;
    }

    if (!ch->operators.count(c.id)) {
        s.sendNumeric(c, 482, chName + " :You're not channel operator");
        return;
    }

    Client* target = s.findClientByNick(targetNick);
    if (!target || !ch->members.count(target->id)) {
        s.sendNumeric(c, 441,
            targetNick + " " + chName + " :They aren't on that channel");
        return;
    }

    const bool hasComment = msg.params.size() >= 3;
    const std::string comment = hasComment
        ? Utils::stripCRLF(msg.params[2], 300)
        : c.nick;

    std::string line;
    line.append(":")
        .append(c.nick)
        .append("!")
        .append(c.user)
        .append("@")
        .append(c.host)
        .append(" KICK ")
        .append(chName)
        .append(" ")
        .append(target->nick)
        .append(" :")
        .append(comment)
        .append("\r\n");

    s.broadcastChannel(chName, line);
    s.removeClientFromChannel(*target, *ch);
}
static AutoRegisterCommand<CommandKick> s_reg_kick("KICK");
