#include "m_whowas.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"
#include <ctime>

void CommandWhowas::execute(Server& s, Client& c, const Message& msg) const
{
    // WHOWAS <nick>{,<nick>} [<count>] [<server>]
    if (!c.registered) {
        s.sendNumeric(c, 451, ":You have not registered");
        return;
    }
    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, 431, ":No nickname given");
        return;
    }

    // Optional <count>: >0 caps entries per nick; absent/<=0 means all.
    long limit = 0;
    if (msg.params.size() >= 2 && !msg.params[1].empty()) {
        long v = 0;
        bool ok = true;
        for (char ch : msg.params[1]) {
            if (ch < '0' || ch > '9') { ok = false; break; }
            v = v * 10 + (ch - '0');
            if (v > 1000) { v = 1000; break; }
        }
        if (ok) limit = v;
    }

    std::vector<std::string> targets = Utils::splitCsv(msg.params[0]);
    constexpr size_t MAX_TARGETS = 5;
    if (targets.size() > MAX_TARGETS) targets.resize(MAX_TARGETS);

    for (const std::string& target : targets) {
        if (target.empty()) continue;

        const std::string key = Utils::ircCaseFold(target);
        const auto* dq = s.whowas().lookup(key);
        if (!dq) {
            s.sendNumeric(c, 406, target + " :There was no such nickname");
            s.sendNumeric(c, 369, target + " :End of WHOWAS");
            continue;
        }

        size_t emitted = 0;
        for (const auto& e : *dq) {
            if (limit > 0 && emitted >= static_cast<size_t>(limit)) break;

            s.sendNumeric(c, 314, e.nick + " " + e.user + " " + e.host + " * :" + e.realname); // RPL_WHOWASUSER

            char when[64];
            std::time_t t = e.signoff;
            std::tm tmv{};
#if defined(_WIN32)
            gmtime_s(&tmv, &t);
#else
            gmtime_r(&t, &tmv);
#endif
            std::strftime(when, sizeof(when), "%a %b %d %H:%M:%S %Y", &tmv);

            s.sendNumeric(c, 312, e.nick + " " + e.serverName + " :" + when); // RPL_WHOISSERVER

            ++emitted;
        }

        s.sendNumeric(c, 369, target + " :End of WHOWAS"); // RPL_ENDOFWHOWAS
    }
}
static AutoRegisterCommand<CommandWhowas> s_reg_whowas("WHOWAS");
