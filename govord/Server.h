#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <vector>

#include "SocketCompat.h"
#include "config.h"
#include "Channel.h"
#include "Client.h"
#include "Motd.h"
#include "Whowas.h"

#include <openssl/ssl.h>

class TlsContext
{
public:
    TlsContext(const std::string& certPath, const std::string& keyPath);
    ~TlsContext();
    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;
    TlsContext(TlsContext&&) = delete;
    TlsContext& operator=(TlsContext&&) = delete;
    SSL_CTX* get() const noexcept { return ctx_; }
private:
    SSL_CTX* ctx_ = nullptr;
};

void platformNetInit();
void platformNetShutdown() noexcept;

class Server 
{
public:
    using LineHandler = std::function<void(Client&, std::string_view line)>;

    Server(const std::string& port,
        const std::string& certPath,
        const std::string& keyPath,
        std::string serverName = DEFAULT_SERVER_NAME,
        std::string network = DEFAULT_NETWORK,
        std::string description = DEFAULT_DESCRIPTION);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop() { stopRequested_ = true; }
    void setLineHandler(LineHandler h) { lineHandler_ = std::move(h); }

    bool sendTo(int cid, std::string_view data);
    bool sendTo(std::string_view nick, std::string_view data);
    bool sendTo(Client& c, std::string_view data) { return sendTo(c.id, data); }

    void broadcastChannel(const std::string& ch, std::string_view msg);
    void broadcastChannel(const std::string& ch, std::string_view msg, const Client* except);

    void closeClient(int cid, std::string_view reason = {});
    void removeClientFromAllChannels(Client* c);

    Client* findClient(int cid);
    const std::unordered_map<int, Client>& clients() const { return clients_; }
    int     findCidByNick(std::string_view nick);
    Client* findClientByNick(std::string_view nick);
    bool    nickExists(std::string_view nick);
    Client* findClientById(int cid) { return findClient(cid); }
    std::chrono::steady_clock::time_point now() const {
        return std::chrono::steady_clock::now();
    }

    void registerNick(std::string_view nick, int cid);
    void unregisterNick(std::string_view nick);

    const std::string& name() const { return serverName_; }
    const std::string& network() const { return network_; }
    const std::string& description() const { return description_; }

    void completeRegistration(Client& c);
    void tryCompleteRegistration(Client& c);

    void loadMotd(const std::string& path) { motd_.load(path); }
    void sendMotd(Client& c);

    void sendNumeric(Client& c, int code, const std::string& message);

    Channel* getChannel(const std::string& name);
    Channel& getOrCreateChannel(const std::string& name);
    void     eraseChannel(const std::string& name);
    bool     isBanned(const Channel& ch, const Client& c) const;

    void setPassword(std::string p) { requiredPass_ = std::move(p); }
    const std::string& password() const { return requiredPass_; }

    void partAllChannels(Client& c, const std::string& reason = "Leaving");

    void succeedOpIfNeeded(Channel& ch);
    void removeClientFromChannel(Client& c, Channel& ch);

    void recordWhowas(const Client& c) { whowas_.record(c, serverName_); }
    const WhowasStore& whowas() const { return whowas_; }

    const std::unordered_map<std::string, Channel>& channels() const noexcept { return channels_; }

private:
    void acceptOne();
    void serviceClient(Client& c, short revents);
    void doHandshake(Client& c);
    void doRead(Client& c);
    void doWrite(Client& c);
    void processBuffer(Client& c);
    void teardownClient(Client& c);
    void reapTimeouts();
    void openListener(const std::string& port);

    TlsContext      tls_;
    Motd            motd_;
    WhowasStore     whowas_;

    socket_t        listenSock_     = kInvalidSocket;
    bool            stopRequested_  = false;
    int             nextCid_        = 1;
    std::string     serverName_     = DEFAULT_SERVER_NAME;
    std::string     network_        = DEFAULT_NETWORK;
    std::string     description_    = DEFAULT_DESCRIPTION;
    std::string     requiredPass_;

    std::unordered_map<int, Client>      clients_;
    std::unordered_map<std::string, int> nicks_;

    LineHandler     lineHandler_;
    std::chrono::steady_clock::time_point lastReapAt_;

    std::unordered_map<std::string, Channel> channels_;
};