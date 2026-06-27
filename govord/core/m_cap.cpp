#include "m_cap.h"
#include "../Server.h"
#include "IrcMessage.h"
#include <format>
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>

static const std::vector<std::string> SUPPORTED_CAPS = {
    "cap-notify",
    "invite-notify",
};

// caps that, once ack'd, cannot be disabled by the client
static bool isSticky(std::string_view name, bool cap302) 
{
    if (cap302 && name == "cap-notify") return true;
    return false;
}

static bool isSupportedCap(std::string_view name)
{
    for (const auto& entry : SUPPORTED_CAPS) {
        std::string_view e(entry);
        auto eq = e.find('=');
        std::string_view base = (eq == std::string_view::npos) ? e : e.substr(0, eq);
        if (base == name) return true;
    }
    return false;
}

static std::vector<std::string_view> tokenize(std::string_view s) 
{
    std::vector<std::string_view> out;
    size_t pos = 0;
    while (pos < s.size()) {
        // skip runs of spaces
        while (pos < s.size() && s[pos] == ' ') ++pos;
        if (pos >= s.size()) break;
        size_t sp = s.find(' ', pos);
        size_t end = (sp == std::string_view::npos) ? s.size() : sp;
        out.emplace_back(s.substr(pos, end - pos));
        pos = end;
    }
    return out;
}

static std::string buildLsPayload() 
{
    std::string caps;
    for (size_t i = 0; i < SUPPORTED_CAPS.size(); ++i) {
        if (i) caps += ' ';
        caps += SUPPORTED_CAPS[i];
    }
    return caps;
}

static void sendLs(Server& s, Client& c, std::string_view nick, bool cap302) 
{
    std::string payload = buildLsPayload();

    if (!cap302) {
        s.sendTo(c, std::format(":{} CAP {} LS :{}\r\n", s.name(), nick, payload));
        return;
    }

    constexpr size_t MAX_PAYLOAD = 400;

    auto tokens = tokenize(payload);
    std::vector<std::string> lines;
    std::string cur;
    for (auto tok : tokens) {
        size_t needed = cur.empty() ? tok.size() : cur.size() + 1 + tok.size();
        if (needed > MAX_PAYLOAD && !cur.empty()) {
            lines.push_back(std::move(cur));
            cur.clear();
        }
        if (!cur.empty()) cur += ' ';
        cur.append(tok);
    }
    if (!cur.empty()) lines.push_back(std::move(cur));
    if (lines.empty()) lines.emplace_back();

    for (size_t i = 0; i + 1 < lines.size(); ++i)
        s.sendTo(c, std::format(":{} CAP {} LS * :{}\r\n", s.name(), nick, lines[i]));
    s.sendTo(c, std::format(":{} CAP {} LS :{}\r\n", s.name(), nick, lines.back()));
}

void CommandCap::execute(Server& s, Client& c, const Message& msg) const
{
    std::string_view nick = c.nick.empty() ? std::string_view("*") : std::string_view(c.nick);

    if (msg.params.empty()) {
        s.sendTo(c, std::format(":{} 461 {} CAP :Not enough parameters\r\n", s.name(), nick));
        return;
    }

    std::string sub(msg.params[0]);
    std::transform(sub.begin(), sub.end(), sub.begin(),
        [](unsigned char ch) { return std::toupper(ch); });

    if (sub == "LS") {
        bool cap302 = false;
        if (msg.params.size() >= 2) {
            std::string_view v = msg.params[1];
            int version = 0;
            for (char ch : v) {
                if (ch < '0' || ch > '9') { version = 0; break; }
                version = version * 10 + (ch - '0');
                if (version > 1000000) break;
            }
            if (version >= 302) cap302 = true;
        }
        if (cap302) c.cap302 = true;

        if (!c.registered) c.capNegotiating = true;

        sendLs(s, c, nick, c.cap302);
        return;
    }

    if (sub == "LIST") {
        std::string caps;
        bool first = true;
        for (const auto& cap : c.enabledCaps) {
            if (!first) caps += ' ';
            caps += cap;
            first = false;
        }

        s.sendTo(c, std::format(":{} CAP {} LIST :{}\r\n", s.name(), nick, caps));
        return;
    }

    if (sub == "REQ") {
        if (msg.params.size() < 2) {
            s.sendTo(c, std::format(":{} 461 {} CAP :Not enough parameters\r\n", s.name(), nick));
            return;
        }
        if (!c.registered) c.capNegotiating = true;

        std::string_view req = msg.params[1];
        std::vector<std::pair<std::string, bool>> requested; // (name, enabling?)
        bool allValid = true;

        for (auto tok : tokenize(req)) {
            bool enabling = true;
            if (!tok.empty() && tok.front() == '-') {
                enabling = false;
                tok.remove_prefix(1);
            }
            if (tok.empty()) { allValid = false; continue; }

            std::string name(tok);

            if (!isSupportedCap(name))
                allValid = false;
            else if (!enabling && isSticky(name, c.cap302))
                allValid = false;

            requested.emplace_back(std::move(name), enabling);
        }

        if (!allValid) {
            s.sendTo(c, std::format(":{} CAP {} NAK :{}\r\n", s.name(), nick, req));
            return;
        }

        for (auto& [name, enabling] : requested) {
            if (enabling) c.enabledCaps.insert(name);
            else c.enabledCaps.erase(name);
        }
        s.sendTo(c, std::format(":{} CAP {} ACK :{}\r\n", s.name(), nick, req));
        return;
    }

    if (sub == "END") {
        if (!c.registered) {
            c.capNegotiating = false;
            s.tryCompleteRegistration(c);
        }
        return;
    }

    if (sub == "CLEAR") {
        std::string ackList;
        std::vector<std::string> toRemove;
        for (const auto& cap : c.enabledCaps) {
            if (isSticky(cap, c.cap302)) continue;
            toRemove.push_back(cap);
        }
        for (const auto& cap : toRemove) {
            c.enabledCaps.erase(cap);
            if (!ackList.empty()) ackList += ' ';
            ackList += '-';
            ackList += cap;
        }
        s.sendTo(c, std::format(":{} CAP {} ACK :{}\r\n", s.name(), nick, ackList));
        return;
    }

    s.sendTo(c, std::format(":{} 410 {} {} :Invalid CAP command\r\n", s.name(), nick, sub));
}
static AutoRegisterCommand<CommandCap> s_reg_cap("CAP");
