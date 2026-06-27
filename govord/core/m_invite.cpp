#include "m_invite.h"
#include "IrcMessage.h"
#include "../Server.h"

void CommandInvite::execute(Server& s, Client& c, const Message& msg) const
{
    // INVITE <nick> <channel>
    if (msg.params.size() < 2 || msg.params[0].empty() || msg.params[1].empty()) {
        s.sendNumeric(c, 461, "INVITE :Not enough parameters");
        return;
    }

    const std::string& targetNick = msg.params[0];
    const std::string& chName = msg.params[1];

    Client* target = s.findClientByNick(targetNick);
    if (!target || !target->registered) {
        s.sendNumeric(c, 401, targetNick + " :No such nick/channel");
        return;
    }

    Channel* ch = s.getChannel(chName);
    if (!ch) {
        s.sendNumeric(c, 403, chName + " :No such channel");
        return;
    }

    if (!ch->members.count(c.id)) {
        s.sendNumeric(c, 442, chName + " :You're not on that channel");
        return;
    }

    if (ch->members.count(target->id)) {
        s.sendNumeric(c, 443, targetNick + " " + chName + " :is already on channel");
        return;
    }

    if (ch->inviteOnly && !ch->operators.count(c.id)) {
        s.sendNumeric(c, 482, chName + " :You're not channel operator");
        return;
    }

    const auto now = s.now();
    if (now - c.lastInviteAt < std::chrono::seconds(1)) {
        s.sendNumeric(c, 263, "INVITE :Please wait before sending another invite");
        return;
    }
    c.lastInviteAt = now;

    constexpr size_t MAX_INVITES_PER_CHANNEL = 100;
    if (ch->invited.size() >= MAX_INVITES_PER_CHANNEL) {
        s.sendNumeric(c, 263, "INVITE :Channel invite list full");
        return;
    }

    ch->invited.insert(target->id);

    s.sendNumeric(c, 341, targetNick + " " + chName);

    if (!target->awayMessage.empty()) {
        s.sendNumeric(c, 301, targetNick + " :" + target->awayMessage);
    }

    const std::string prefix = ":" + c.nick + "!" + c.user + "@" + c.host;
    s.sendTo(*target, prefix + " INVITE " + target->nick + " :" + chName + "\r\n");

    std::string inviteLine;
    inviteLine.append(prefix)
        .append(" INVITE ")
        .append(target->nick)
        .append(" :")
        .append(chName)
        .append("\r\n");

    std::string noticeLine;
    noticeLine.append(":")
        .append(std::string(s.name()))
        .append(" NOTICE @")
        .append(chName)
        .append(" :*** ")
        .append(c.nick)
        .append(" invited ")
        .append(target->nick)
        .append(" into the channel\r\n");

    for (const auto& memberId : ch->operators) {
        if (memberId == c.id || memberId == target->id) continue;
        Client* op = s.findClientById(memberId);
        if (!op) continue;
        if (op->enabledCaps.count("invite-notify"))
            s.sendTo(*op, inviteLine);
        else
            s.sendTo(*op, noticeLine);
    }
}
static AutoRegisterCommand<CommandInvite> s_reg_invite("INVITE");
