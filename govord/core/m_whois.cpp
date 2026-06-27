#include "m_whois.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"
#include <chrono>

static void whoisOne(Server& s, Client& requester, const std::string& targetNick)
{
    Client* t = s.findClientByNick(targetNick);
    if (!t || !t->registered) {
        s.sendNumeric(requester, 401, targetNick + " :No such nick/channel");
        s.sendNumeric(requester, 318, targetNick + " :End of /WHOIS list");
        return;
    }

    s.sendNumeric(requester, 311, t->nick + " " + t->user + " " + t->host + " * :" + t->realname); // RPL_WHOISUSER

    {
        constexpr size_t LINE_CAP = 400;
        std::string list;
        auto flush = [&] {
            if (!list.empty()) {
                s.sendNumeric(requester, 319, t->nick + " :" + list); // RPL_WHOISCHANNELS
                list.clear();
            }
            };
        for (const Channel* ch : t->channels) {
            if (!ch) continue;
            std::string entry;
            if (ch->operators.count(t->id))   entry = "@";
            else if (ch->voiced.count(t->id)) entry = "+";
            entry += ch->name;

            if (!list.empty() && list.size() + 1 + entry.size() > LINE_CAP)
                flush();
            if (!list.empty()) list += " ";
            list += entry;
        }
        flush();
    }

    s.sendNumeric(requester, 312, t->nick + " " + s.name() + " :" + s.description()); // RPL_WHOISSERVER

    if (t->ssl)
        s.sendNumeric(requester, 671, t->nick + " :is using a secure connection (TLS)"); // RPL_WHOISSECURE (this is)

    // 317 RPL_WHOISIDLE: <nick> <idle> <signon> :seconds idle, signon time
    const auto sysNow = std::chrono::system_clock::now();
    const auto steadyNow = std::chrono::steady_clock::now();

    long long idle = std::chrono::duration_cast<std::chrono::seconds>(
        steadyNow - t->lastActiveAt).count();
    if (idle < 0) idle = 0;

    const auto signonSys = sysNow -
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            steadyNow - t->connectedAt);
    const long long signonUnix =
        std::chrono::duration_cast<std::chrono::seconds>(
            signonSys.time_since_epoch()).count();

    s.sendNumeric(requester, 317,
        t->nick + " " + std::to_string(idle) + " " +
        std::to_string(signonUnix) + " :seconds idle, signon time");

    s.sendNumeric(requester, 318, t->nick + " :End of /WHOIS list"); // RPL_ENDOFWHOIS
}

void CommandWhois::execute(Server& s, Client& c, const Message& msg) const
{
    if (!c.registered) {
        s.sendNumeric(c, 451, ":You have not registered");
        return;
    }
    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, 431, ":No nickname given");
        return;
    }

    // "WHOIS <targets>" or "WHOIS <server> <targets>": last param is targets.
    const std::string& targetParam =
        msg.params.size() >= 2 ? msg.params[1] : msg.params[0];

    std::vector<std::string> targets = Utils::splitCsv(targetParam);
    constexpr size_t MAX_TARGETS = 5;
    if (targets.size() > MAX_TARGETS) targets.resize(MAX_TARGETS);

    for (const std::string& t : targets) {
        if (t.empty()) continue;
        whoisOne(s, c, t);
    }
}
static AutoRegisterCommand<CommandWhois> s_reg_whois("WHOIS");
