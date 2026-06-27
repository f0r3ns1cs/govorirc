#include "Ui.h"
#include "Socket.h"
#include "Cli.h"
#include "Utils.h"
#include "config.h"

#include "IrcMessage.h"

#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <print>
#include <string>
#include <string_view>
#include <vector>

static std::atomic<bool> g_stop{false};

static void onSignal(int)
{
    g_stop.store(true, std::memory_order_relaxed);
}

static void installSignalHandlers() 
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#ifndef _WIN32
    std::signal(SIGHUP, onSignal);
#endif
}

struct Args 
{
    std::string host;
    std::string port;
    std::string nick;
    std::string user;
    std::string realname;
    std::string password;
    bool passwordSet = false;
    bool nonInteractive = false;
    bool insecure = false;
};

static void usage(const char* prog) 
{
    std::println(stderr,
        "usage: {} <host> <port> [options]\n"
        "  -n, --nick <nick>      set nickname\n"
        "  -u, --user <user>      set username\n"
        "  -r, --real <realname>  set realname\n"
        "  -p, --pass <password>  set server password\n"
        "  -y, --yes              use defaults, don't prompt\n"
        "  -k, --insecure         no TLS cert verification\n"
        "  -h, --help             show help",
        prog);
}

static bool parseArgs(int argc, char** argv, Args& out) 
{
    if (argc < 3)
        return false;
    out.host = argv[1];
    out.port = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string_view a = argv[i];

        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::println(stderr, "{} requires an argument", name);
                return nullptr;
            }
            return argv[++i];
        };

        if (a == "-n" || a == "--nick") {
            const char* v = next("--nick");
            if (!v) return false;
            out.nick = v;
        } 
        else if (a == "-u" || a == "--user") {
            const char* v = next("--user");
            if (!v) return false;
            out.user = v;
        } 
        else if (a == "-r" || a == "--real") {
            const char* v = next("--real");
            if (!v) return false;
            out.realname = v;
        } 
        else if (a == "-p" || a == "--pass") {
            const char* v = next("--pass");
            if (!v) return false;
            out.password = v;
            out.passwordSet = true;
        } 
        else if (a == "-y" || a == "--yes") {
            out.nonInteractive = true;
        } 
        else if (a == "-k" || a == "--insecure") {
            out.insecure = true;
        } 
        else if (a == "-h" || a == "--help") {
            return false;
        } 
        else {
            std::println(stderr, "unknown option: {}", a);
            return false;
        }
    }
    return true;
}

static void gatherIdentity(Args& a)
{
    std::print("host: {}:{}\n\n", a.host, a.port);

    if (a.nick.empty()) {
        std::string def = Cli::defaultNick();
        for (;;) {
            std::string n = a.nonInteractive ? def : Cli::prompt("nickname", def);
            if (Utils::isValidNick(n)) {
                a.nick = n;
                break;
            }
            std::print("  invalid nickname\n");
            if (a.nonInteractive) {
                a.nick = def;
                break;
            }
        }
    } 
    else if (!Utils::isValidNick(a.nick)) {
        std::println(stderr, "invalid nickname: {}", a.nick);
        std::exit(2);
    }

    if (a.user.empty())
        a.user = a.nonInteractive ? Cli::defaultUser(a.nick)
                                  : Cli::prompt("username", Cli::defaultUser(a.nick));
    a.user = Cli::sanitizeUsername(a.user);
    if (a.user.empty())
        a.user = Cli::sanitizeUsername(a.nick);

    if (a.realname.empty())
        a.realname = a.nonInteractive ? "govorirc user"
                                      : Cli::prompt("real name", "govorirc user");
    a.realname = Cli::sanitizeRealname(a.realname);
    if (a.realname.empty())
        a.realname = "govorirc user";

    if (!a.passwordSet && !a.nonInteractive) {
        a.password = Cli::promptHidden("server password");
        a.passwordSet = !a.password.empty();
    }

    std::print("\nregistering as:\n");
    std::print("  nick:     {}\n", a.nick);
    std::print("  user:     {}\n", a.user);
    std::print("  realname: {}\n", a.realname);
    std::print("  password: {}\n\n", a.passwordSet ? "(set)" : "(none)");
}

static void sendHandshake(Socket& sock, const Args& a) 
{
    if (a.passwordSet && !a.password.empty())
        sock.send(std::format("PASS {}\r\n", a.password));
    sock.send("CAP LS 302\r\n");
    sock.send(std::format("NICK {}\r\n", a.nick));
    sock.send(std::format("USER {} 0 * :{}\r\n", a.user, a.realname));
    sock.send("CAP END\r\n");
}

static bool isNumeric(std::string_view cmd)
{
    return cmd.size() == 3 &&
           std::isdigit(static_cast<unsigned char>(cmd[0])) &&
           std::isdigit(static_cast<unsigned char>(cmd[1])) &&
           std::isdigit(static_cast<unsigned char>(cmd[2]));
}

static bool hasChan(const std::vector<std::string>& v, std::string_view ch)
{
    for (const auto& j : v)
        if (Utils::sameNick(j, ch)) return true;
    return false;
}
static void removeChan(std::vector<std::string>& v, std::string_view ch)
{
    std::erase_if(v, [&](const std::string& j) { return Utils::sameNick(j, ch); });
}

static bool handleLocalCommand(Ui& ui, std::string& currentTarget, const std::vector<std::string>& joined, std::string_view line)
{
    while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
    if (line.empty() || line.front() != '/') return false;
    line.remove_prefix(1);

    const std::string cmd = Utils::toUpper(line.substr(0, line.find(' ')));
    if (cmd != "WIN" && cmd != "SWITCH" && cmd != "W")
        return false;

    std::string_view arg;
    if (size_t sp = line.find(' '); sp != std::string_view::npos) {
        arg = line.substr(sp + 1);
        while (!arg.empty() && arg.front() == ' ') arg.remove_prefix(1);
    }
    const std::string_view target = arg.substr(0, arg.find(' '));

    if (target.empty()) {
        if (joined.empty()) {
            ui.addLine("* not in any channels");
        } else {
            std::string list = "* channels:";
            for (const auto& ch : joined) {
                list += ' ';
                if (Utils::sameNick(ch, currentTarget)) list += '*';  // '*' = active
                list += ch;
            }
            ui.addLine(std::move(list));
        }
        ui.addLine(std::format("* current target: {}",
                   currentTarget.empty() ? "(none)" : currentTarget));
        return true;
    }

    if (Utils::isChannelName(target) && !hasChan(joined, target)) {
        ui.addLine(std::format("* you are not on {}", target));
        return true;
    }

    currentTarget = std::string(target);
    ui.addLine(std::format("* now talking in {}", currentTarget));
    return true;
}

static void onServerLine(Ui& ui, Socket& sock, std::string& nick, std::string& currentTarget, std::vector<std::string>& joined, std::string_view line)
{
    Message m = tokenizeMsg(line);
    if (!m.valid)
        return;

    const std::string& cmd = m.command;
    const std::string who = m.prefix ? m.prefix->nick : "";

    auto param = [&](std::size_t i) -> std::string_view {
        return i < m.params.size() ? std::string_view(m.params[i]) : std::string_view{};
    };
    auto paren = [](std::string_view s) {
        return s.empty() ? std::string{} : std::format(" ({})", s);
    };

    if (cmd == "PING") {
        sock.send(std::format("PONG :{}\r\n", param(0)));
    }
    else if (cmd == "PRIVMSG" || cmd == "NOTICE") {
        if (m.params.size() < 2)
            return;

        // prepends channel origin to message for ease of tracking message
        std::string ctx;
        if (Utils::isChannelName(param(0)))
            ctx = std::format("[{}] ", param(0));

        std::string_view text = m.params[1];
        if (text.starts_with("\x01""ACTION ")) {
            text.remove_prefix(8);
            if (!text.empty() && text.back() == '\x01')
                text.remove_suffix(1);
            ui.addLine(std::format("{}* {} {}", ctx, who, text));
        }
        else if (cmd == "PRIVMSG") {
            ui.addLine(std::format("{}<{}> {}", ctx, who, text));
        }
        else {
            ui.addLine(std::format("{}-{}- {}", ctx, who, text));
        }
    }
    else if (cmd == "JOIN") {
        if (who == nick && !param(0).empty()) {
            currentTarget = std::string(param(0));
            if (!hasChan(joined, param(0)))
                joined.emplace_back(param(0));
        }
        ui.addLine(std::format("* {} joined {}", who, param(0)));
    }
    else if (cmd == "PART") {
        if (who == nick) {
            removeChan(joined, param(0));
            if (Utils::sameNick(param(0), currentTarget))
                currentTarget = joined.empty() ? std::string() : joined.back();
        }
        ui.addLine(std::format("* {} left {}{}", who, param(0), paren(param(1))));
    }
    else if (cmd == "QUIT") {
        ui.addLine(std::format("* {} quit{}", who, paren(param(0))));
    }
    else if (cmd == "KICK") {
        if (param(1) == nick) {
            removeChan(joined, param(0));
            if (Utils::sameNick(param(0), currentTarget))
                currentTarget = joined.empty() ? std::string() : joined.back();
        }
        ui.addLine(std::format("* {} kicked {} from {}{}",
                               who, param(1), param(0), paren(param(2))));
    }
    else if (cmd == "NICK") {
        if (who == nick)
            nick = std::string(param(0));
        ui.addLine(std::format("* {} is now known as {}", who, param(0)));
    }
    else if (cmd == "TOPIC") {
        ui.addLine(std::format("* {} changed topic of {} to: {}",
                               who, param(0), param(1)));
    }
    else if (cmd == "MODE") {
        std::string modes;
        for (std::size_t i = 1; i < m.params.size(); ++i) {
            if (!modes.empty()) modes += ' ';
            modes += m.params[i];
        }
        ui.addLine(std::format("* {} sets mode {} on {}", who, modes, param(0)));
    }
    else if (cmd == "353") { // RPL_NAMREPLY: <me> = #chan :nicks
        ui.addLine(std::format("* users in {}: {}", param(2), param(3)));
    }
    else if (cmd == "332") { // RPL_TOPIC: <me> #chan :topic
        ui.addLine(std::format("* topic for {}: {}", param(1), param(2)));
    }
    else if (cmd == "331") { // RPL_NOTOPIC: <me> #chan :no topic
        ui.addLine(std::format("* {}: no topic set", param(1)));
    }
    else if (cmd == "322") { // RPL_LIST: <me> #chan count :topic
        ui.addLine(std::format("{} ({} users) {}", param(1), param(2), param(3)));
    }
    else if (cmd == "366" || cmd == "321" || cmd == "323") {
        // end-of-NAMES, LIST start/end: nothing useful
    }
    else if (isNumeric(cmd)) {
        // single-payload numerics (welcome, MOTD, errors)
        if (!m.params.empty())
            ui.addLine(m.params.back());
    }
    else {
        std::string out = cmd;
        for (const auto& p : m.params) {
            out += ' ';
            out += p;
        }
        ui.addLine(out);
    }
}

int main(int argc, char* argv[]) {
    Args args;
    if (!parseArgs(argc, argv, args)) {
        usage(argv[0]);
        return 1;
    }

    installSignalHandlers();
    gatherIdentity(args);

    Ui ui;
    ui.setStatus(std::format("connecting to {}:{} as {} ...", args.host, args.port, args.nick));

    if (args.insecure)
        ui.addLine("\x03""07warning: TLS certificate verification disabled (--insecure)\x0F");

    Socket sock(args.host, args.port, args.insecure);
    if (sock.closed()) {
        ui.addLine("\x03""04connection failed\x0F");
        ui.setStatus("disconnected");
        while (!g_stop.load(std::memory_order_relaxed) && !ui.shouldStop()) {
            ui.tick();
            ui.napIfIdle(100);
        }
        return 1;
    }

    ui.setStatus(std::format("connected to {}:{} as {}  (type /quit to exit)", args.host, args.port, args.nick));

    std::string currentTarget;
    std::vector<std::string> joined;

    sock.setLineHandler([&](std::string_view line) {
        onServerLine(ui, sock, args.nick, currentTarget, joined, line);
    });

    ui.setLineHandler([&](std::string_view line) {
        if (line.empty())
            return;
        if (sock.closed()) {
            ui.addLine("\x03""04not connected\x0F");
            return;
        }

        if (handleLocalCommand(ui, currentTarget, joined, line))
            return;
        std::string out = formatOutbound(line, currentTarget);
        if (out.empty())
            return;

        // catch /join before sending over to server so currentTarget stays accurate
        if (out.starts_with("JOIN ")) {
            std::string_view rest(out);
            rest.remove_prefix(5);
            std::string_view chans = rest.substr(0, rest.find(' '));
            std::string_view first = chans.substr(0, chans.find(','));
            if (!first.empty() && first != "0")
                currentTarget = std::string(first);
        }
        sock.send(std::format("{}\r\n", out));
    });

    sendHandshake(sock, args);

    while (!g_stop.load(std::memory_order_relaxed) && !ui.shouldStop() && !sock.closed()) {
        sock.poll(50);
        ui.tick();
    }

    if (g_stop.load(std::memory_order_relaxed) && !sock.closed()) {
        sock.send("QUIT :Caught signal\r\n");
        sock.poll(200);
    }

    return 0;
}