#include "m_names.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"

static void sendNamesFor(Server& s, Client& c, const Channel& ch)
{
    constexpr size_t LINE_BUDGET = 400;
    std::string list;
    list.reserve(LINE_BUDGET);

    auto flush = [&] {
        if (list.empty()) return;
        s.sendNumeric(c, 353, "= " + ch.name + " :" + list);
        list.clear();
        };

    for (int cid : ch.members) {
        const Client* u = s.findClient(cid);
        if (!u) continue;

        std::string entry;
        if (ch.operators.count(cid))   entry = "@";
        else if (ch.voiced.count(cid)) entry = "+";
        entry += u->nick;

        if (!list.empty() && list.size() + 1 + entry.size() > LINE_BUDGET)
            flush();
        if (!list.empty()) list += " ";
        list += entry;
    }
    flush();

    s.sendNumeric(c, 366, ch.name + " :End of /NAMES list");
}

void CommandNames::execute(Server& s, Client& c, const Message& msg) const
{
    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, 366, "* :End of /NAMES list");
        return;
    }

    for (const std::string& name : Utils::splitCsv(msg.params[0])) {
        if (name.empty()) continue;
        Channel* ch = s.getChannel(name);
        if (!ch) {
            s.sendNumeric(c, 366, name + " :End of /NAMES list");
            continue;
        }
        sendNamesFor(s, c, *ch);
    }
}
static AutoRegisterCommand<CommandNames> s_reg_names("NAMES");
