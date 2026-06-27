#pragma once
#include <string>
#include <deque>
#include <unordered_map>
#include <ctime>
#include <cstddef>

struct Client;

struct WhowasEntry 
{
    std::string nick, user, host, realname, serverName;
    std::time_t signoff = 0;
};

class WhowasStore 
{
public:
    void record(const Client& c, std::string_view serverName);
    const std::deque<WhowasEntry>* lookup(const std::string& foldedKey) const;
private:
    static constexpr size_t MAX_PER_NICK = 8;
    static constexpr size_t MAX_TOTAL    = 4096;
    std::unordered_map<std::string, std::deque<WhowasEntry>> history_;
    std::deque<std::string> order_;
};