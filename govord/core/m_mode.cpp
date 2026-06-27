#include "m_mode.h"
#include "IrcMessage.h"
#include "../Server.h"
#include "Utils.h"
#include <cctype>

struct ModeView 
{
    std::string flags;
    std::vector<std::string> args;
};

static ModeView describeChannel(const Channel& ch, bool viewerIsMember)
{
    ModeView v;
    v.flags = "+";
    if (ch.inviteOnly)      v.flags += 'i';
    if (ch.moderated)       v.flags += 'm';
    if (ch.topicRestricted) v.flags += 't';
    if (!ch.key.empty())    v.flags += 'k';
    if (ch.userLimit > 0)   v.flags += 'l';
    if (v.flags == "+") v.flags.clear();

    if (!ch.key.empty())  v.args.push_back(viewerIsMember ? ch.key : "*");
    if (ch.userLimit > 0) v.args.push_back(std::to_string(ch.userLimit));
    return v;
}

static void sendChannelModeIs(Server& s, Client& c, const Channel& ch) 
{
    const bool member = ch.members.count(c.id) > 0;
    ModeView v = describeChannel(ch, member);
    std::string payload = ch.name;
    if (!v.flags.empty()) {
        payload += " " + v.flags;
        for (const auto& a : v.args) payload += " " + a;
    }
    s.sendNumeric(c, 324, payload);
}

static void sendBanList(Server& s, Client& c, const Channel& ch) 
{
    for (const auto& mask : ch.banMasks)
        s.sendNumeric(c, 367, ch.name + " " + mask); // RPL_BANLIST
    s.sendNumeric(c, 368, ch.name + " :End of channel ban list");
}

struct Applied 
{
    char sign;
    char mode;
    std::string arg;
};

static bool consumesArg(char sign, char mode) noexcept
{
    switch (mode) {
    case 'k': return sign == '+';
    case 'l': return sign == '+';
    case 'o': return true;
    case 'v': return true;
    case 'b': return true;
    default:  return false;
    }
}

static bool parsePositiveLimit(const std::string& s, size_t& out) 
{
    if (s.empty()) return false;
    size_t v = 0;
    for (char ch : s) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
        v = v * 10 + static_cast<size_t>(ch - '0');
        if (v > 100000) return false;
    }
    if (v == 0) return false;
    out = v;
    return true;
}

static std::string normalizeBanMask(std::string m)
{
    if (m.empty()) return "*!*@*";
    const bool hasBang = m.find('!') != std::string::npos;
    const bool hasAt = m.find('@') != std::string::npos;
    if (!hasBang && !hasAt)      m = m + "!*@*"; // nick only
    else if (!hasBang)           m = "*!" + m; // user@host, no nick
    else if (!hasAt)             m = m + "@*"; // nick!user, no host
    return m;
}

void CommandMode::execute(Server& s, Client& c, const Message& msg) const
{
    // MODE <target> [<modestring> [<args>...]]
    if (msg.params.empty()) {
        s.sendNumeric(c, 461, "MODE :Not enough parameters");
        return;
    }

    const std::string& target = msg.params[0];

    // user mode
    if (!Utils::isChannelName(target)) {
        if (!Utils::sameNick(target, c.nick)) {
            s.sendNumeric(c, 502, ":Cannot change mode for other users");
            return;
        }
        s.sendNumeric(c, 221, "+"); // no user modes tracked
        return;
    }

    // channel mode
    Channel* chPtr = s.getChannel(target);
    if (!chPtr) {
        s.sendNumeric(c, 403, target + " :No such channel");
        return;
    }
    Channel& ch = *chPtr;

    // query
    if (msg.params.size() < 2) {
        sendChannelModeIs(s, c, ch);
        return;
    }

    const std::string& modestr = msg.params[1];

    // "MODE #foo b" with no mask is a ban-list query: allowed for any client without op.
    if ((modestr == "b" || modestr == "+b") && msg.params.size() < 3) {
        sendBanList(s, c, ch);
        return;
    }

    if (!ch.operators.count(c.id)) {
        s.sendNumeric(c, 482, target + " :You're not channel operator");
        return;
    }

    size_t argIdx = 2;
    std::vector<Applied> applied;
    char sign = '+';

    for (char m : modestr) {
        if (m == '+') { sign = '+'; continue; }
        if (m == '-') { sign = '-'; continue; }

        if (m != 'i' && m != 't' && m != 'k' && m != 'l' && m != 'o' &&
            m != 'b' && m != 'm' && m != 'v') {
            std::string emsg;
            emsg += m;
            emsg += " :is unknown mode char to me";
            s.sendNumeric(c, 472, emsg);
            continue;
        }

        std::string arg;
        if (consumesArg(sign, m)) {
            if (argIdx >= msg.params.size()) {
                // +b with no remaining arg: list
                if (m == 'b' && sign == '+') { sendBanList(s, c, ch); }
                continue;
            }
            arg = msg.params[argIdx++];
        }

        switch (m) {
        case 'i': {
            bool want = (sign == '+');
            if (ch.inviteOnly == want) break;
            ch.inviteOnly = want;
            applied.push_back({ sign, 'i', {} });
            break;
        }
        case 't': {
            bool want = (sign == '+');
            if (ch.topicRestricted == want) break;
            ch.topicRestricted = want;
            applied.push_back({ sign, 't', {} });
            break;
        }
        case 'm': {
            bool want = (sign == '+');
            if (ch.moderated == want) break;
            ch.moderated = want;
            applied.push_back({ sign, 'm', {} });
            break;
        }
        case 'k': {
            if (sign == '+') {
                if (arg.empty()) break;
                ch.key = arg;
                applied.push_back({ '+', 'k', arg });
            }
            else {
                if (ch.key.empty()) break;
                ch.key.clear();
                applied.push_back({ '-', 'k', {} });
            }
            break;
        }
        case 'l': {
            if (sign == '+') {
                size_t lim = 0;
                if (!parsePositiveLimit(arg, lim)) break;
                if (ch.userLimit == lim) break;
                ch.userLimit = lim;
                applied.push_back({ '+', 'l', arg });
            }
            else {
                if (ch.userLimit == 0) break;
                ch.userLimit = 0;
                applied.push_back({ '-', 'l', {} });
            }
            break;
        }
        case 'o': {
            Client* tgt = s.findClientByNick(arg);
            if (!tgt || !ch.members.count(tgt->id)) {
                s.sendNumeric(c, 441,
                    arg + " " + target + " :They aren't on that channel");
                break;
            }
            if (sign == '+') {
                if (ch.operators.insert(tgt->id).second)
                    applied.push_back({ '+', 'o', tgt->nick });
            }
            else {
                if (ch.operators.erase(tgt->id) > 0)
                    applied.push_back({ '-', 'o', tgt->nick });
            }
            break;
        }
        case 'v': {
            Client* tgt = s.findClientByNick(arg);
            if (!tgt || !ch.members.count(tgt->id)) {
                s.sendNumeric(c, 441,
                    arg + " " + target + " :They aren't on that channel");
                break;
            }
            if (sign == '+') {
                if (ch.voiced.insert(tgt->id).second)
                    applied.push_back({ '+', 'v', tgt->nick });
            }
            else {
                if (ch.voiced.erase(tgt->id) > 0)
                    applied.push_back({ '-', 'v', tgt->nick });
            }
            break;
        }
        case 'b': {
            std::string mask = normalizeBanMask(arg);
            if (sign == '+') {
                if (!ch.banMasks.count(mask) && ch.banMasks.size() >= MAX_BAN_LIST) {
                    s.sendNumeric(c, 478,
                        target + " " + mask + " :Channel ban list is full");
                    break;
                }
                if (ch.banMasks.insert(mask).second)
                    applied.push_back({ '+', 'b', mask });
            }
            else {
                if (ch.banMasks.erase(mask) > 0)
                    applied.push_back({ '-', 'b', mask });
            }
            break;
        }
        }
    }

    if (applied.empty()) return;

    std::string flags;
    std::string argsTail;
    char lastSign = 0;
    for (const auto& a : applied) {
        if (a.sign != lastSign) { flags += a.sign; lastSign = a.sign; }
        flags += a.mode;
        if (!a.arg.empty()) argsTail += " " + a.arg;
    }

    std::string line =
        ":" + c.nick + "!" + c.user + "@" + c.host +
        " MODE " + target + " " + flags + argsTail + "\r\n";

    s.broadcastChannel(target, line);
}
static AutoRegisterCommand<CommandMode> s_reg_mode("MODE");
