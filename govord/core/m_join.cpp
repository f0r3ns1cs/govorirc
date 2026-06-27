#include "m_join.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

void CommandJoin::execute(Server& s, Client& c, const Message& msg) const
{
    // JOIN <channel>{,<channel>} [<key>{,<key>}]
    // JOIN 0 ; PARTs user from all joined channels
    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, 461, "JOIN :Not enough parameters");
        return;
    }

    if (msg.params[0] == "0") {
        s.partAllChannels(c);
        return;
    }

    constexpr size_t MAX_JOINS_PER_COMMAND = 10;

    std::vector<std::string> channels = Utils::split(msg.params[0], ',');
    std::vector<std::string> keys;
    if (msg.params.size() > 1)
        keys = Utils::split(msg.params[1], ',');

    if (channels.size() > MAX_JOINS_PER_COMMAND)
        channels.resize(MAX_JOINS_PER_COMMAND);

    for (size_t i = 0; i < channels.size(); ++i) {
        const std::string& chName = channels[i];
        const std::string key = (i < keys.size()) ? keys[i] : "";

        if (!Utils::isValidChannelName(chName)) {
            s.sendNumeric(c, 403, chName + " :No such channel");
            continue;
        }

        if (c.channels.size() >= MAX_CHANNELS_PER_USER) {
            s.sendNumeric(c, 405, chName + " :You have joined too many channels");
            continue;
        }

        const bool created = (s.getChannel(chName) == nullptr);
        Channel& ch = s.getOrCreateChannel(chName);

        if (ch.members.count(c.id))
            continue;

        if (!created) {
            const bool invited = ch.invited.count(c.id) > 0;

            if (s.isBanned(ch, c) && !invited) {
                s.sendNumeric(c, 474, chName + " :Cannot join channel (+b)");
                continue;
            }

            if (ch.inviteOnly && !invited) {
                s.sendNumeric(c, 473, chName + " :Cannot join channel (+i)");
                continue;
            }

            if (!ch.key.empty() && ch.key != key && !invited) {
                s.sendNumeric(c, 475, chName + " :Cannot join channel (+k)");
                continue;
            }

            if (ch.userLimit > 0 && ch.members.size() >= ch.userLimit && !invited) {
                s.sendNumeric(c, 471, chName + " :Cannot join channel (+l)");
                continue;
            }
        }

        if (ch.members.size() >= MAX_USERS_PER_CHANNEL) {
            s.sendNumeric(c, 471, chName + " :Cannot join channel (channel is full)");
            continue;
        }

        ch.members.insert(c.id);
        ch.joinOrder[c.id] = ch.nextJoinSeq++;
        if (created)
            ch.operators.insert(c.id);
        c.channels.push_back(&ch);

        ch.invited.erase(c.id);

        std::string joinMsg;
        joinMsg.append(":")
               .append(c.nick)
               .append("!")
               .append(c.user)
               .append("@")
               .append(c.host)
               .append(" JOIN :")
               .append(chName)
               .append("\r\n");
        s.broadcastChannel(chName, joinMsg);

        if (!ch.topic.empty()) {
            s.sendNumeric(c, 332, chName + " :" + ch.topic);
            if (!ch.topicSetter.empty() && ch.topicTime != 0) {
                s.sendNumeric(c, 333,
                    chName + " " + ch.topicSetter + " " +
                    std::to_string(ch.topicTime));
            }
        }
        else {
            s.sendNumeric(c, 331, chName + " :No topic is set");
        }

        constexpr size_t NAMES_PAYLOAD_BUDGET = 400;
        std::string names;
        for (int cid : ch.members) {
            const Client* u = s.findClient(cid);
            if (!u) continue;
            std::string entry;
            if (ch.operators.count(cid)) entry += '@';
            entry += u->nick;

            const size_t needed = names.empty() ? entry.size() : names.size() + 1 + entry.size();
            if (needed > NAMES_PAYLOAD_BUDGET) {
                s.sendNumeric(c, 353, "= " + chName + " :" + names);
                names.clear();
            }
            if (!names.empty()) names += ' ';
            names += entry;
        }
        if (!names.empty())
            s.sendNumeric(c, 353, "= " + chName + " :" + names);
        s.sendNumeric(c, 366, chName + " :End of /NAMES list");
    }
}
static AutoRegisterCommand<CommandJoin> s_reg_join("JOIN");
