#include "m_notice.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

void CommandNotice::execute(Server& s, Client& c, const Message& msg) const
{
    // NOTICE <target>{,<target>} :<text>
    if (!c.registered)         return;
    if (msg.params.size() < 2) return;
    if (msg.params[0].empty()) return;
    if (msg.params[1].empty()) return;

    const std::string text = Utils::stripCRLF(msg.params[1], 400);
    std::vector<std::string> targets = Utils::splitCsv(msg.params[0]);
    if (targets.empty()) return;

    const std::string prefix =
        ":" + c.nick + "!" + c.user + "@" + c.host + " NOTICE ";

    for (const std::string& target : targets) {
        if (target.empty()) continue;

        if (Utils::isChannelName(target)) {
            Channel* ch = s.getChannel(target);
            if (!ch) continue;

            const bool isMember = ch->members.count(c.id) > 0;

            if (!isMember) continue;

            if (ch->moderated &&
                !ch->operators.count(c.id) &&
                !ch->voiced.count(c.id)) {
                continue;
            }

            if (s.isBanned(*ch, c) &&
                !ch->operators.count(c.id) &&
                !ch->voiced.count(c.id)) continue;

            std::string line = prefix + target + " :" + text + "\r\n";
            s.broadcastChannel(target, line, &c);
        }
        else {
            Client* dst = s.findClientByNick(target);
            if (!dst) continue;
            if (!dst->registered) continue;

            std::string line;
            line.append(prefix)
                .append(dst->nick)
                .append(" :")
                .append(text)
                .append("\r\n");
            s.sendTo(*dst, line);
        }
    }
}
static AutoRegisterCommand<CommandNotice> s_reg_notice("NOTICE");
