#include "Whowas.h"
#include "Client.h"
#include "Utils.h"

void WhowasStore::record(const Client& c, std::string_view serverName) 
{
    if (c.nick.empty()) return;
    const std::string key = Utils::ircCaseFold(c.nick);
    auto& dq = history_[key];
    dq.push_front(WhowasEntry{ c.nick, c.user, c.host, c.realname,
                               std::string(serverName), std::time(nullptr) });
    order_.push_front(key);
    if (dq.size() > MAX_PER_NICK)
        dq.pop_back();
    while (order_.size() > MAX_TOTAL) {
        const std::string old = std::move(order_.back());
        order_.pop_back();
        auto it = history_.find(old);
        if (it != history_.end() && !it->second.empty()) {
            it->second.pop_back();
            if (it->second.empty()) history_.erase(it);
        }
    }
}

const std::deque<WhowasEntry>*
WhowasStore::lookup(const std::string& foldedKey) const 
{
    auto it = history_.find(foldedKey);
    if (it == history_.end() || it->second.empty()) return nullptr;
    return &it->second;
}