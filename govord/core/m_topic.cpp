#include "m_topic.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

#include <ctime>
#include <algorithm>

void CommandTopic::execute(Server& s, Client& c, const Message& msg) const
{
    // TOPIC <channel>          ; getter
    // TOPIC <channel> :<text>  ; setter

    if (msg.params.empty()) {
        s.sendNumeric(c, 461, "TOPIC :Not enough parameters");
        return;
    }

    const std::string& chName = msg.params[0];

    Channel* ch = s.getChannel(chName);
    if (!ch) {
        s.sendNumeric(c, 403, chName + " :No such channel");
        return;
    }

    // getter
    if (msg.params.size() < 2) {
        if (ch->topic.empty()) {
            s.sendNumeric(c, 331, chName + " :No topic is set");
        }
        else {
            s.sendNumeric(c, 332, chName + " :" + ch->topic);

            // 333 RPL_TOPICWHOTIME: only if we have a setter recorded.
            if (!ch->topicSetter.empty() && ch->topicTime != 0) {
                s.sendNumeric(c, 333,
                    chName + " " + ch->topicSetter + " " +
                    std::to_string(static_cast<long long>(ch->topicTime)));
            }
        }
        return;
    }


    // setter

    if (!ch->members.count(c.id)) {
        s.sendNumeric(c, 442, chName + " :You're not on that channel");
        return;
    }

    if (ch->topicRestricted && !ch->operators.count(c.id)) {
        s.sendNumeric(c, 482, chName + " :You're not channel operator");
        return;
    }

    std::string newTopic = Utils::stripCRLF(msg.params[1], 390);

    ch->topic = newTopic;
    ch->topicSetter = c.nick;
    ch->topicTime = std::time(nullptr);

    std::string line;
    line.append(":")
        .append(c.nick)
        .append("!")
        .append(c.user)
        .append("@")
        .append(c.host)
        .append(" TOPIC ")
        .append(chName)
        .append(" :")
        .append(newTopic)
        .append("\r\n");
    s.broadcastChannel(chName, line);
}
static AutoRegisterCommand<CommandTopic> s_reg_topic("TOPIC");
