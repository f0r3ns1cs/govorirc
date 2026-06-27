#include "IrcMessage.h"
#include "Utils.h"

#include <format>

static std::string unescapeTagValue(std::string_view v)
{
    std::string out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] != '\\') { out += v[i]; continue; }
        if (i + 1 >= v.size()) break;
        
        switch (v[++i]) {
        case ':':  out += ';';  break;
        case 's':  out += ' ';  break;
        case '\\': out += '\\'; break;
        case 'r':  out += '\r'; break;
        case 'n':  out += '\n'; break;
        default:   out += v[i]; break;
        }
    }
    return out;
}

static std::string escapeTagValue(std::string_view v)
{
    std::string out;
    out.reserve(v.size());
    for (char c : v) {
        switch (c) {
        case ';':  out += "\\:";  break;
        case ' ':  out += "\\s";  break;
        case '\\': out += "\\\\"; break;
        case '\r': out += "\\r";  break;
        case '\n': out += "\\n";  break;
        default:   out += c;      break;
        }
    }
    return out;
}


Message tokenizeMsg(std::string_view msg)
{
    // [@tags] [:prefix] <command> [params...] [:trailing]
    Message out;
    if (msg.empty()) return out;

    // Strip trailing CRLF
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) {
        msg.remove_suffix(1);
    }

    // Strip leading spaces
    while (!msg.empty() && msg.front() == ' ') {
        msg.remove_prefix(1);
    }

    if (msg.empty()) return out;

    auto consumeToken = [&]() -> std::string_view {
        size_t sp = msg.find(' ');
        std::string_view tok = msg.substr(0, sp);
        msg.remove_prefix(sp == std::string_view::npos ? msg.size() : sp);
        while (!msg.empty() && msg.front() == ' ')
            msg.remove_prefix(1);
        return tok;
        };

    // Tags (optional): '@' key[=value][;key[=value]]...
    if (!msg.empty() && msg.front() == '@') {
        auto tok = consumeToken();
        std::vector<Tag> outTags;
        std::string_view tags = tok.substr(1); // drop '@'
        while (!tags.empty()) {
            size_t sp = tags.find(';');
            std::string_view tagTok = tags.substr(0, sp);
            tags.remove_prefix(sp == std::string_view::npos ? tags.size() : sp + 1);
            if (tagTok.empty()) continue;
            size_t eq = tagTok.find('=');
            if (eq == std::string_view::npos)
                outTags.push_back({ std::string(tagTok), {} });
            else
                outTags.push_back({ std::string(tagTok.substr(0, eq)),
                                    unescapeTagValue(tagTok.substr(eq + 1)) });
        }
        out.tags = std::move(outTags);

        // just tags is invalid
        if (msg.empty()) return out;
    }

    // Prefix (optional): ':' nick[!user][@host]  OR  ':' server
    if (!msg.empty() && msg.front() == ':') {
        auto tok = consumeToken();
        std::string_view prefix = tok.substr(1); // drop ':'
        Prefix p;
        size_t bang = prefix.find('!');
        size_t at = prefix.find('@');

        if (bang == std::string_view::npos && at == std::string_view::npos) {
            p.isServer = prefix.find('.') != std::string_view::npos;
            p.nick = prefix;
        }
        else {
            p.isServer = false;
            if (bang != std::string_view::npos) {
                p.nick = prefix.substr(0, bang);
                if (at != std::string_view::npos) {
                    p.user = prefix.substr(bang + 1, at - bang - 1);
                    p.host = prefix.substr(at + 1);
                }
                else {
                    p.user = prefix.substr(bang + 1);
                }
            }
            else { // '@' but no '!'
                p.nick = prefix.substr(0, at);
                p.host = prefix.substr(at + 1);
            }
        }
        out.prefix = p;

        // no command is invalid
        if (msg.empty()) return out;
    }

    // Command (required)
    out.command = consumeToken();
    if (out.command.empty())
        return out;

    // Params: up to 15, last allows spaces
    while (!msg.empty()) {
        if (msg.front() == ':' || out.params.size() == 14) {
            if (msg.front() == ':') msg.remove_prefix(1);
            out.params.emplace_back(msg);
            break;
        }
        out.params.emplace_back(consumeToken());
    }

    out.valid = true;
    return out;
}

std::string serializeMsg(const Message& msg)
{
    std::string out;

    if (msg.tags && !msg.tags->empty()) {
        out += '@';
        bool first = true;
        for (const Tag& t : *msg.tags) {
            if (!first) out += ';';
            first = false;
            out += t.k;
            if (!t.v.empty()) {
                out += '=';
                out += escapeTagValue(t.v);
            }
        }
        out += ' ';
    }

    if (msg.prefix) {
        const Prefix& p = *msg.prefix;
        out += ':';
        out += p.nick;
        if (!p.user.empty()) { out += '!'; out += p.user; }
        if (!p.host.empty()) { out += '@'; out += p.host; }
        out += ' ';
    }

    out += msg.command;

    for (size_t i = 0; i < msg.params.size(); ++i) {
        const std::string& p = msg.params[i];
        const bool last = (i + 1 == msg.params.size());
        out += ' ';
        if (last && (p.empty() || p.front() == ':' ||
                     p.find(' ') != std::string::npos))
            out += ':';
        out += p;
    }

    return out;
}

std::string formatOutbound(std::string_view input, std::string_view currentTarget)
{
    std::string out;

    if (input.empty()) return out;

    while (!input.empty() && input.front() == ' ') {
        input.remove_prefix(1);
    }
    if (input.empty()) return out;

    if (input.front() != '/') {
        // no slash: PRIVMSG <current> :text
        out = std::format("PRIVMSG {} :{}", currentTarget, input);
    }
    else if (input.size() >= 2 && input[1] == '/') {
        // literal message starting with a slash
        input.remove_prefix(1);
        out = std::format("PRIVMSG {} :{}", currentTarget, input);
    }
    else {
        auto nextToken = [](std::string_view& sv) -> std::string_view {
            while (!sv.empty() && sv.front() == ' ') sv.remove_prefix(1);
            size_t n = sv.find(' ');
            std::string_view tok = sv.substr(0, n);
            sv.remove_prefix(n == std::string_view::npos ? sv.size() : n);
            return tok;
        };
        auto trimFront = [](std::string_view& sv) {
            while (!sv.empty() && sv.front() == ' ') sv.remove_prefix(1);
        };

        input.remove_prefix(1); // drop leading '/'
        std::string c = Utils::toUpper(nextToken(input));

        if (c == "MSG") {
            std::string_view target = nextToken(input);
            trimFront(input);
            out = std::format("PRIVMSG {} :{}", target, input);
        }
        else if (c == "ME") {
            trimFront(input);
            out = std::format("PRIVMSG {} :\x01""ACTION {}\x01", currentTarget, input);
        }
        else {
            // passthrough: JOIN, WHOIS, QUIT, unknowns
            trimFront(input);
            out = input.empty() ? c : std::format("{} {}", c, input);
        }
    }

    return out;
}