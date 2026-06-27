#include "m_who.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"
#include <unordered_set>

static std::string sharedChannel(const Client& requester, const Client& target)
{
    for (const Channel* tch : target.channels) {
        if (!tch) continue;
        // When +s exists: skip secret channels the requester isn't in.
        if (tch->members.count(requester.id))
            return tch->name;
    }
    return "*";
}

static void sendWhoReply(Server& s, Client& requester, const Client& target,
    const std::string& channelCtx, const Channel* ctxCh)
{
    // Flags: H (here) / G (gone/away).
    std::string flags = "H";
    if (ctxCh) {
        if (ctxCh->operators.count(target.id))   flags += "@";
        else if (ctxCh->voiced.count(target.id)) flags += "+";
    }

    // 352 RPL_WHOREPLY:
    // <channel> <user> <host> <server> <nick> <flags> :<hopcount> <realname>
    s.sendNumeric(requester, 352,
        channelCtx + " " +
        target.user + " " +
        target.host + " " +
        s.name() + " " +
        target.nick + " " +
        flags + " :0 " +
        target.realname);
}

void CommandWho::execute(Server& s, Client& c, const Message& msg) const
{
    if (!c.registered) {
        s.sendNumeric(c, 451, ":You have not registered");
        return;
    }

    if (msg.params.empty() || msg.params[0].empty()) {
        s.sendNumeric(c, 315, "* :End of /WHO list");
        return;
    }

    const std::string& mask = msg.params[0];
    const bool opersOnly = (msg.params.size() > 1 && msg.params[1] == "o");

    // Channel WHO
    if (Utils::isChannelName(mask)) {
        Channel* ch = s.getChannel(mask);
        if (ch) {
            for (int cid : ch->members) {
                Client* t = s.findClient(cid);
                if (!t || !t->registered) continue;
                if (opersOnly && !t->isOper) continue;
                sendWhoReply(s, c, *t, mask, ch);
            }
        }
        s.sendNumeric(c, 315, mask + " :End of /WHO list");
        return;
    }

    const bool fullMask =
        mask.find('!') != std::string::npos ||
        mask.find('@') != std::string::npos;
    const bool hasWildcard =
        mask.find('*') != std::string::npos ||
        mask.find('?') != std::string::npos;

    // exact nick, no wildcards, no host component
    if (!hasWildcard && !fullMask) {
        Client* t = s.findClientByNick(mask);
        if (t && t->registered && !(opersOnly && !t->isOper)) {
            const std::string ctx = sharedChannel(c, *t);
            const Channel* ctxCh = (ctx != "*") ? s.getChannel(ctx) : nullptr;
            sendWhoReply(s, c, *t, ctx, ctxCh);
        }
        s.sendNumeric(c, 315, mask + " :End of /WHO list");
        return;
    }

    // wildcards
    for (const auto& [cid, client] : s.clients()) {
        (void)cid;
        const Client* t = &client;
        if (!t->registered) continue;
        if (opersOnly && !t->isOper) continue;

        bool match;
        if (fullMask) {
            const std::string mstr =
                t->nick + "!" + t->user + "@" + t->host;
            match = Utils::globMatch(mask, mstr);
        }
        else {
            match = Utils::globMatch(mask, t->nick) ||
                Utils::globMatch(mask, t->host);
        }
        if (!match) continue;

        const std::string ctx = sharedChannel(c, *t);
        const Channel* ctxCh = (ctx != "*") ? s.getChannel(ctx) : nullptr;
        sendWhoReply(s, c, *t, ctx, ctxCh);
    }

    s.sendNumeric(c, 315, mask + " :End of /WHO list");
}
static AutoRegisterCommand<CommandWho> s_reg_who("WHO");
