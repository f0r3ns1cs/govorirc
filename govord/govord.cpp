#include "Server.h"
#include "config.h"
#include "CommandRegistry.h"
#include "IrcMessage.h"
#include "Utils.h"
#include <atomic>
#include <csignal>
#include <exception>
#include <print>
#include <set>
#include <string>
#include <format>
#include <string_view>

std::atomic<bool> g_stop{ false };
Server* g_server = nullptr;

static void onSignal(int)
{
    g_stop.store(true, std::memory_order_relaxed);
    if (g_server) g_server->stop();
}

static void installSignalHandlers()
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#ifndef _WIN32
    std::signal(SIGHUP, onSignal);
#endif
}

static const std::set<std::string>& preRegAllowed()
{
    static const std::set<std::string> s = {
        "CAP", "PASS", "NICK", "USER", "QUIT", "PING", "PONG"
    };
    return s;
}

int main(int argc, char* argv[])
{
    const std::string port = argc > 1 ? argv[1] : "6697",
        certPath = argc > 2 ? argv[2] : "cert.pem",
        keyPath = argc > 3 ? argv[3] : "key.pem",
        serverName = argc > 4 ? argv[4] : DEFAULT_SERVER_NAME,
        network = argc > 5 ? argv[5] : DEFAULT_NETWORK,
        motdPath = "motd.txt";

    try {
        Server server(port, certPath, keyPath, serverName, network);
        g_server = &server;

        installSignalHandlers();

        server.loadMotd(motdPath);

        server.setLineHandler([&](Client& c, std::string_view line) {
            Message msg = tokenizeMsg(line);
            if (msg.command.empty()) return;

            const std::string cmdName = Utils::toUpper(msg.command);

            if (!c.registered && !preRegAllowed().contains(cmdName)) {
                server.sendTo(c, std::format(
                    ":{} 451 * :You have not registered\r\n",
                    server.name()));
                return;
            }

            auto cmd = CommandRegistry::instance().create(cmdName);
            if (!cmd) {
                server.sendTo(c, std::format(
                    ":{} 421 {} {} :Unknown command\r\n",
                    server.name(),
                    c.nick.empty() ? "*" : c.nick,
                    cmdName));
                return;
            }

            if (msg.params.size() < cmd->minParams()) {
                server.sendTo(c, std::format(
                    ":{} 461 {} {} :Not enough parameters\r\n",
                    server.name(),
                    c.nick.empty() ? "*" : c.nick,
                    cmdName));
                return;
            }

            cmd->execute(server, c, msg);
            });

        std::println(stderr, "govord {} ({}) listening on port {}",
            serverName, network, port);
        std::println(stderr, "cert: {}", certPath);

        server.run();

        std::println(stderr, "shutting down");
    }
    catch (const std::exception& e) {
        std::println(stderr, "fatal: {}", e.what());
        return 1;
    }
    return 0;
}