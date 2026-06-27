#include "m_part.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

void CommandPart::execute(Server& s, Client& c, const Message& msg) const
{
    // PART <channel>{,<channel>} [:<reason>]
    if (msg.params.empty()) {
        s.sendNumeric(c, 461, "PART :Not enough parameters");
        return;
    }

    std::vector<std::string> channels = Utils::splitCsv(msg.params[0]);
    if (channels.empty()) {
        s.sendNumeric(c, 461, "PART :Not enough parameters");
        return;
    }

    const bool hasReason = msg.params.size() >= 2;
    std::string reason = hasReason ? Utils::stripCRLF(msg.params[1], 300) : std::string();

    for (const std::string& chName : channels)
    {
        Channel* ch = s.getChannel(chName);
        if (!ch) {
            s.sendNumeric(c, 403, chName + " :No such channel");
            continue;
        }

        if (!ch->members.count(c.id)) {
            s.sendNumeric(c, 442, chName + " :You're not on that channel");
            continue;
        }

        std::string line;
        line.append(":")
            .append(c.nick)
            .append("!")
            .append(c.user)
            .append("@")
            .append(c.host)
            .append(" PART ")
            .append(chName);
        if (hasReason) line += " :" + reason;
        line += "\r\n";

        s.broadcastChannel(chName, line);

        s.removeClientFromChannel(c, *ch);
    }
}
static AutoRegisterCommand<CommandPart> s_reg_part("PART");
