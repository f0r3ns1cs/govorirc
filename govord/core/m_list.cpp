#include "m_list.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

static bool isSecret(const Channel& /*ch*/) { return false; }
static bool isPrivate(const Channel& /*ch*/) { return false; }

static void emitListLine(Server& s, Client& c, const Channel& ch)
{
    // 322 RPL_LIST: <channel> <#visible-users> :<topic>
    // Secret/private channels show 0 users and no topic to non-members.
    const bool member = ch.members.count(c.id) > 0;
    const std::string userCount =
        (isPrivate(ch) && !member) ? "0" : std::to_string(ch.members.size());
    const std::string topic =
        (isPrivate(ch) && !member) ? "" : ch.topic;

    s.sendNumeric(c, 322, ch.name + " " + userCount + " :" + topic);
}

void CommandList::execute(Server& s, Client& c, const Message& msg) const
{
    // LIST [<channel>{,<channel>}] [<server>] ; server param ignored (single-server IRCd).
    if (!c.registered) {
        s.sendNumeric(c, 451, ":You have not registered");
        return;
    }

    s.sendNumeric(c, 321, "Channel :Users  Name"); // RPL_LISTSTART

    if (msg.params.empty() || msg.params[0].empty()) {
        for (const auto& [name, ch] : s.channels()) {
            if ((isSecret(ch) || isPrivate(ch)) && !ch.members.count(c.id))
                continue;
            emitListLine(s, c, ch);
        }
    }
    else {
        for (const std::string& name : Utils::splitCsv(msg.params[0])) {
            Channel* ch = s.getChannel(name);
            if (!ch) continue;
            if ((isSecret(*ch) || isPrivate(*ch)) && !ch->members.count(c.id))
                continue;
            emitListLine(s, c, *ch);
        }
    }
    
    s.sendNumeric(c, 323, ":End of /LIST"); // RPL_LISTEND
}
static AutoRegisterCommand<CommandList> s_reg_list("LIST");
