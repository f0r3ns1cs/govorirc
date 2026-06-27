#include "Server.h"
#include "Utils.h"
#include "Numerics.h"

#include <algorithm>
#include <format>
#include <print>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <csignal>

#include <openssl/err.h>

static std::string peerHost(socket_t s) 
{
    sockaddr_storage addr{};
    socklen_type len = sizeof(addr);
    if (::getpeername(s, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
        return "unknown";
    char buf[NI_MAXHOST];
    if (::getnameinfo(reinterpret_cast<sockaddr*>(&addr), len, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST) != 0)
        return "unknown";
    return buf;
}

static std::string drainSslErrors(const char* what) 
{
    std::string out;
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        char buf[256]{};
        ERR_error_string_n(e, buf, sizeof(buf));
        out += std::format("{}: {}\n", what, buf);
    }
    return out;
}

void platformNetInit() 
{
#ifdef _WIN32
    WSADATA wsa;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw std::runtime_error("WSAStartup failed");
#else
    ::signal(SIGPIPE, SIG_IGN);
#endif
}

void platformNetShutdown() noexcept 
{
#ifdef _WIN32
    ::WSACleanup();
#endif
}

TlsContext::TlsContext(const std::string& certPath, const std::string& keyPath) 
{
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_)
        throw std::runtime_error(drainSslErrors("SSL_CTX_new"));

    SSL_CTX_set_mode(ctx_, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    if (SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION) != 1 ||
        SSL_CTX_use_certificate_chain_file(ctx_, certPath.c_str()) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx_, keyPath.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_check_private_key(ctx_) != 1) {
        const std::string e = drainSslErrors("SSL_CTX setup");
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error(e);
    }
}

TlsContext::~TlsContext() 
{
    if (ctx_)
        SSL_CTX_free(ctx_);
}

Server::Server(const std::string& port,
               const std::string& certPath,
               const std::string& keyPath,
               std::string serverName,
               std::string network,
               std::string description)
    : tls_(certPath, keyPath)
    , serverName_(std::move(serverName))
    , network_(std::move(network))
    , description_(std::move(description))
{
    platformNetInit();
    openListener(port);
    lastReapAt_ = std::chrono::steady_clock::now();
}

Server::~Server() 
{
    for (auto& [cid, c] : clients_) {
        if (c.ssl) { SSL_shutdown(c.ssl); SSL_free(c.ssl); }
        closeSock(c.sock);
    }
    clients_.clear();
    nicks_.clear();
    closeSock(listenSock_);
    platformNetShutdown();
}

void Server::openListener(const std::string& port)
{
    // use ipv6 if supported, fallback to ipv4
    auto tryFamily = [&](int family) -> socket_t {
        addrinfo hints{};
        hints.ai_family   = family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags    = AI_PASSIVE;

        addrinfo* addr = nullptr;
        if (::getaddrinfo(nullptr, port.c_str(), &hints, &addr) != 0 || !addr)
            return kInvalidSocket;

        socket_t s = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (s == kInvalidSocket) { ::freeaddrinfo(addr); return kInvalidSocket; }

        setReuseAddr(s);
        if (family == AF_INET6) {
            const int off = 0;
            ::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));
        }

        if (!setNonBlocking(s) ||
            ::bind(s, addr->ai_addr, static_cast<socklen_type>(addr->ai_addrlen)) != 0 ||
            ::listen(s, SOMAXCONN) != 0) {
            ::freeaddrinfo(addr);
            closeSock(s);
            return kInvalidSocket;
        }
        ::freeaddrinfo(addr);
        return s;
    };

    listenSock_ = tryFamily(AF_INET6);
    if (listenSock_ == kInvalidSocket)
        listenSock_ = tryFamily(AF_INET);
    if (listenSock_ == kInvalidSocket)
        throw std::runtime_error(std::format("failed to open listener on port {}: {}", port, sockErrorString(lastSockError())));
}

void Server::run() 
{
    std::vector<pollfd_t> pfds;
    std::vector<int>      cids;
    pfds.reserve(64);
    cids.reserve(64);

    while (!stopRequested_) {
        pfds.clear();
        cids.clear();
        pfds.push_back({ listenSock_, POLLIN, 0 });
        cids.push_back(-1);

        for (auto& [cid, c] : clients_) {
            short events = 0;
            if (c.wantRead)  events |= POLLIN;
            if (c.wantWrite || !c.sendBuf.empty()) events |= POLLOUT;
            pfds.push_back({ c.sock, events, 0 });
            cids.push_back(cid);
        }

        int n = pollSockets(pfds.data(), pfds.size(), 1000);
        if (n < 0) {
            if (isInterrupted(lastSockError())) continue;
            std::println(stderr, "poll: {}", sockErrorString(lastSockError()));
            break;
        }

        if (pfds[0].revents & POLLIN) acceptOne();

        struct Ready { int cid; short revents; };
        std::vector<Ready> ready;
        ready.reserve(pfds.size() - 1);
        for (std::size_t i = 1; i < pfds.size(); ++i) {
            if (pfds[i].revents == 0) continue;
            ready.push_back({ cids[i], pfds[i].revents });
        }

        for (const auto& r : ready) {
            auto it = clients_.find(r.cid);
            if (it == clients_.end()) continue;
            serviceClient(it->second, r.revents);
        }

        for (auto it = clients_.begin(); it != clients_.end(); ) {
            if (it->second.markedForClose) {
                teardownClient(it->second);
                it = clients_.erase(it);
            } 
            else {
                ++it;
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - lastReapAt_ > std::chrono::seconds(5)) {
            reapTimeouts();
            lastReapAt_ = now;
        }
    }
}

void Server::acceptOne() 
{
    sockaddr_storage addr{};
    socklen_type len = sizeof(addr);
    socket_t s = ::accept(listenSock_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (s == kInvalidSocket) return;

    if (clients_.size() >= MAX_CONNECTIONS) { closeSock(s); return; }

    if (!setNonBlocking(s)) { closeSock(s); return; }
    setTcpNoDelay(s);

    SSL* ssl = SSL_new(tls_.get());
    if (!ssl) { closeSock(s); return; }
    SSL_set_fd(ssl, static_cast<int>(s));
    SSL_set_accept_state(ssl);

    const int cid = nextCid_++;
    Client c;
    c.id = cid;
    c.sock = s;
    c.ssl = ssl;
    c.host = peerHost(s);
    c.connectedAt = std::chrono::steady_clock::now();
    c.lastActiveAt = c.connectedAt;
    c.wantRead = true;
    auto [it, ok] = clients_.emplace(cid, std::move(c));
    if (!ok) {
        // cid collision (just drop connection)
        SSL_free(ssl);
        closeSock(s);
    }
}

void Server::serviceClient(Client& c, short revents) 
{
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        closeClient(c.id, "connection closed");
        return;
    }
    if (!c.handshakeDone) {
        doHandshake(c);
        return;
    }
    if (revents & POLLIN)  doRead(c);
    if (!c.markedForClose && (revents & POLLOUT)) doWrite(c);
}

void Server::doHandshake(Client& c) 
{
    int r = SSL_accept(c.ssl);
    if (r == 1) {
        c.handshakeDone = true;
        c.wantRead = true;
        c.wantWrite = false;
        return;
    }
    int err = SSL_get_error(c.ssl, r);
    if (err == SSL_ERROR_WANT_READ)  { c.wantRead = true;  c.wantWrite = false; return; }
    if (err == SSL_ERROR_WANT_WRITE) { c.wantRead = false; c.wantWrite = true;  return; }
    closeClient(c.id, "TLS handshake failed");
}

void Server::doRead(Client& c) 
{
    char buf[MAX_RECVBUFLEN];
    for (;;) {
        int n = SSL_read(c.ssl, buf, sizeof(buf));
        if (n > 0) {
            if (c.recvBuf.size() + static_cast<std::size_t>(n) > MAX_LINE_LEN * 4) {
                closeClient(c.id, "input flood");
                return;
            }
            c.recvBuf.append(buf, static_cast<std::size_t>(n));
            c.lastActiveAt = std::chrono::steady_clock::now();
            c.pingPending = false;
            continue;
        }
        int err = SSL_get_error(c.ssl, n);
        if (err == SSL_ERROR_WANT_READ)  { c.wantRead = true;  c.wantWrite = false; break; }
        if (err == SSL_ERROR_WANT_WRITE) { c.wantRead = false; c.wantWrite = true;  break; }
        if (err == SSL_ERROR_ZERO_RETURN) { closeClient(c.id, {}); return; }
        closeClient(c.id, "read error");
        return;
    }
    processBuffer(c);
}

void Server::doWrite(Client& c) 
{
    while (!c.sendBuf.empty()) {
        int n = SSL_write(c.ssl, c.sendBuf.data(),
                          static_cast<int>(c.sendBuf.size()));
        if (n > 0) {
            c.sendBuf.erase(0, static_cast<std::size_t>(n));
            continue;
        }
        int err = SSL_get_error(c.ssl, n);
        // write is blocked (waiting on TLS read)
        if (err == SSL_ERROR_WANT_READ)  { c.wantRead = true; c.wantWrite = false; return; }
        if (err == SSL_ERROR_WANT_WRITE) { c.wantWrite = true; return; }
        closeClient(c.id, "write error");
        return;
    }
    c.wantWrite = false;
}

void Server::processBuffer(Client& c) 
{
    for (;;) {
        std::size_t pos = c.recvBuf.find('\n');
        if (pos == std::string::npos) {
            if (c.recvBuf.size() > MAX_LINE_LEN)
                closeClient(c.id, "line too long");
            return;
        }
        std::string line = c.recvBuf.substr(0, pos);
        c.recvBuf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (lineHandler_) lineHandler_(c, line);
        if (c.markedForClose) return;
    }
}

bool Server::sendTo(int cid, std::string_view data) 
{
    auto it = clients_.find(cid);
    if (it == clients_.end()) return false;
    Client& c = it->second;
    if (c.markedForClose) return false;
    if (c.sendBuf.size() + data.size() > MAX_SENDBUFLEN) {
        closeClient(c.id, "send buffer overflow");
        return false;
    }
    c.sendBuf.append(data);
    return true;
}

bool Server::sendTo(std::string_view nick, std::string_view data) 
{
    int cid = findCidByNick(nick);
    if (cid == -1) return false;
    return sendTo(cid, data);
}

void Server::broadcastChannel(const std::string& channelName, std::string_view msg) 
{
    Channel* ch = getChannel(channelName);
    if (!ch) return;
    for (int cid : ch->members)
        sendTo(cid, msg);
}

void Server::broadcastChannel(const std::string& channelName, std::string_view msg, const Client* except)
{
    Channel* ch = getChannel(channelName);
    if (!ch) return;
    for (int cid : ch->members) {
        Client* c = findClient(cid);
        if (!c) continue;
        if (except && c->id == except->id) continue;
        sendTo(cid, msg);
    }
}

void Server::closeClient(int cid, std::string_view reason) 
{
    auto it = clients_.find(cid);
    if (it == clients_.end()) return;
    it->second.markedForClose = true;
    it->second.closeReason = std::string(reason);
}

void Server::teardownClient(Client& c) 
{
    recordWhowas(c);

    if (!c.closeReason.empty() && c.ssl && c.handshakeDone) {
        std::string err = std::format("ERROR :Closing link: {}\r\n", c.closeReason);
        SSL_write(c.ssl, err.data(), static_cast<int>(err.size())); // best-effort
    }

    removeClientFromAllChannels(&c);
    if (!c.nick.empty()) unregisterNick(c.nick);
    if (c.ssl) { SSL_shutdown(c.ssl); SSL_free(c.ssl); c.ssl = nullptr; }
    closeSock(c.sock);
    c.sock = kInvalidSocket;
}

void Server::reapTimeouts() 
{
    const auto now = std::chrono::steady_clock::now();
    std::string ping;
    ping.append(":")
        .append(serverName_)
        .append(" PING :")
        .append(serverName_)
        .append("\r\n");

    for (auto& [cid, c] : clients_) {
        if (c.markedForClose) continue;

        if (!c.registered) {
            if (now - c.connectedAt > std::chrono::seconds(CONNECT_TIMEOUT_SEC))
                closeClient(cid, "Registration timeout");
            continue;
        }

        if (c.pingPending) {
            if (now - c.pingSentAt >
                std::chrono::seconds(PING_TIMEOUT_SEC - PING_INTERVAL_SEC))
                closeClient(cid, "Ping timeout");
        } 
        else if (now - c.lastActiveAt > std::chrono::seconds(PING_INTERVAL_SEC)) {
            sendTo(cid, ping);
            c.pingPending = true;
            c.pingSentAt = now;
        }
    }
}

Client* Server::findClient(int cid) 
{
    auto it = clients_.find(cid);
    return it == clients_.end() ? nullptr : &it->second;
}

int Server::findCidByNick(std::string_view nick) 
{
    auto it = nicks_.find(Utils::ircCaseFold(nick));
    return it == nicks_.end() ? -1 : it->second;
}

Client* Server::findClientByNick(std::string_view nick) 
{
    auto it = nicks_.find(Utils::ircCaseFold(nick));
    return it == nicks_.end() ? nullptr : findClient(it->second);
}

bool Server::nickExists(std::string_view nick)
 {
    return nicks_.contains(Utils::ircCaseFold(nick));
}

void Server::registerNick(std::string_view nick, int cid)
{
    nicks_[Utils::ircCaseFold(nick)] = cid;
}

void Server::unregisterNick(std::string_view nick) 
{
    nicks_.erase(Utils::ircCaseFold(nick));
}

void Server::completeRegistration(Client& c) 
{
    Numerics::sendWelcome(
        [this](Client& cl, std::string_view data) { sendTo(cl, data); },
        c, serverName_, network_);

    // TODO: 005 ISUPPORT / 251-255 LUSERS
    sendMotd(c);
    c.registered = true;
}

void Server::tryCompleteRegistration(Client& c) 
{
    if (c.registered) return;
    if (c.nick.empty()) return;
    if (c.user.empty()) return;
    if (c.capNegotiating) return;

    if (!requiredPass_.empty() &&
        !Utils::constantTimeEquals(c.suppliedPass, requiredPass_)) {
        sendNumeric(c, 464, ":Password incorrect");
        closeClient(c.id, "Bad password");
        c.suppliedPass.clear();
        return;
    }
    c.suppliedPass.clear();
    completeRegistration(c);
}

void Server::partAllChannels(Client& c, const std::string& reason) 
{
    std::vector<Channel*> snapshot = c.channels;

    std::string prefix;
    prefix.append(":")
          .append(c.nick)
          .append("!")
          .append(c.user)
          .append("@")
          .append(c.host)
          .append(" PART ");

    for (Channel* ch : snapshot) {
        if (!ch) continue;
        auto it = channels_.find(Utils::ircCaseFold(ch->name));
        if (it == channels_.end()) continue;
        Channel& real = it->second;
        if (!real.members.count(c.id)) continue;

        std::string line = prefix + real.name + " :" + reason + "\r\n";
        broadcastChannel(real.name, line);

        real.members.erase(c.id);
        real.operators.erase(c.id);
        real.voiced.erase(c.id);
        real.invited.erase(c.id);
        real.joinOrder.erase(c.id);

        if (real.members.empty()) {
            channels_.erase(it);
        }
        else {
            succeedOpIfNeeded(real);
        }
    }
    c.channels.clear();
}

void Server::sendMotd(Client& c) 
{
    if (motd_.empty()) {
        sendNumeric(c, ERR_NOMOTD, ":MOTD File is missing");
        return;
    }
    sendNumeric(c, RPL_MOTDSTART, ":- " + serverName_ + " Message of the day -");
    for (const std::string& line : motd_.lines())
        sendNumeric(c, RPL_MOTD, ":- " + line);
    sendNumeric(c, RPL_ENDOFMOTD, ":End of /MOTD command");
}

void Server::sendNumeric(Client& c, int code, const std::string& message)
{
    Numerics::send(
        [this](Client& cl, std::string_view data) { sendTo(cl, data); },
        c, serverName_, code, message);
}

Channel* Server::getChannel(const std::string& name) 
{
    auto it = channels_.find(Utils::ircCaseFold(name));
    return it == channels_.end() ? nullptr : &it->second;
}

Channel& Server::getOrCreateChannel(const std::string& name) 
{
    const std::string key = Utils::ircCaseFold(name);
    auto it = channels_.find(key);
    if (it != channels_.end())
        return it->second;
    Channel& ch = channels_[key];
    ch.name = name;
    return ch;
}

void Server::eraseChannel(const std::string& name) 
{
    channels_.erase(Utils::ircCaseFold(name));
}

bool Server::isBanned(const Channel& ch, const Client& c) const 
{
    if (ch.banMasks.empty())
        return false;
    const std::string mask = c.nick + "!" + c.user + "@" + c.host;
    for (const std::string& banMask : ch.banMasks)
        if (Utils::globMatch(banMask, mask))
            return true;
    return false;
}

// implemented this for the edge case of situations like a sole operator of a channel
// leaving with the channel on +i; channel gets bricked without succession.
void Server::succeedOpIfNeeded(Channel& ch) 
{
    if (ch.members.empty() || !ch.operators.empty())
        return;

    int chosen = -1;
    std::uint64_t best = 0;
    for (int id : ch.members) {
        auto it = ch.joinOrder.find(id);
        const std::uint64_t seq = (it != ch.joinOrder.end()) ? it->second : UINT64_MAX;
        if (chosen == -1 || seq < best) { chosen = id; best = seq; }
    }
    Client* c = findClient(chosen);
    if (!c) return;
    ch.operators.insert(chosen);
    broadcastChannel(ch.name,
        ":" + serverName_ + " MODE " + ch.name + " +o " + c->nick + "\r\n");
}

void Server::removeClientFromChannel(Client& c, Channel& ch)
{
    ch.members.erase(c.id);
    ch.operators.erase(c.id);
    ch.voiced.erase(c.id);
    ch.invited.erase(c.id);
    ch.joinOrder.erase(c.id);

    auto& chans = c.channels;
    chans.erase(std::remove(chans.begin(), chans.end(), &ch), chans.end());

    if (ch.members.empty())
        eraseChannel(ch.name);
    else
        succeedOpIfNeeded(ch);
}

void Server::removeClientFromAllChannels(Client* c)
{
    if (!c) return;

    // 'name' is guaranteed case-folded
    std::vector<std::string> emptied;
    for (auto& [name, ch] : channels_) {
        const bool wasMember = ch.members.erase(c->id) > 0;
        ch.operators.erase(c->id);
        ch.voiced.erase(c->id);
        ch.invited.erase(c->id);
        ch.joinOrder.erase(c->id);
        if (ch.members.empty())
            emptied.push_back(name);
        else if (wasMember)
            succeedOpIfNeeded(ch); // last op may have just left
    }
    for (const std::string& key : emptied)
        channels_.erase(key);

    c->channels.clear();
}