#include "m_privmsg.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

void CommandPrivmsg::execute(Server& s, Client& c, const Message& msg) const
{
    // PRIVMSG <target>{,<target>} :<text>
    if (!c.registered) {
        s.sendNumeric(c, 451, ":You have not registered");
        return;
    }

    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, 411, ":No recipient given (PRIVMSG)");
        return;
    }

    if (msg.params.size() < 2 || msg.params[1].empty()) {
        s.sendNumeric(c, 412, ":No text to send");
        return;
    }

    std::vector<std::string> targets = Utils::splitCsv(msg.params[0]);
    if (targets.empty()) {
        s.sendNumeric(c, 411, ":No recipient given (PRIVMSG)");
        return;
    }

    std::string text = Utils::stripCRLF(msg.params[1], 400);

    const std::string prefix =
        ":" + c.nick + "!" + c.user + "@" + c.host + " PRIVMSG ";

    for (const std::string& target : targets)
    {
        if (target.empty()) continue;

        if (Utils::isChannelName(target))
        {
            Channel* ch = s.getChannel(target);
            if (!ch) {
                s.sendNumeric(c, 403, target + " :No such channel");
                continue;
            }

            const bool isMember = ch->members.count(c.id) > 0;

            if (!isMember) {
                s.sendNumeric(c, 404,
                    target + " :Cannot send to channel");
                continue;
            }

            if (ch->moderated &&
                !ch->operators.count(c.id) &&
                !ch->voiced.count(c.id))
            {
                s.sendNumeric(c, 404,
                    target + " :Cannot send to channel (+m)");
                continue;
            }

            if (s.isBanned(*ch, c) &&
                !ch->operators.count(c.id) &&
                !ch->voiced.count(c.id)) {
                s.sendNumeric(c, 404,
                    target + " :Cannot send to channel (+b)");
                continue;
            }

            std::string line = prefix + target + " :" + text + "\r\n";
            s.broadcastChannel(target, line);
            continue;
        }

        Client* dst = s.findClientByNick(target);
        if (!dst || !dst->registered) {
            s.sendNumeric(c, 401, target + " :No such nick/channel");
            continue;
        }

        std::string line;
        line.append(prefix)
            .append(dst->nick)
            .append(" :")
            .append(text)
            .append("\r\n");
        s.sendTo(*dst, line);
    }
}
static AutoRegisterCommand<CommandPrivmsg> s_reg_privmsg("PRIVMSG");
