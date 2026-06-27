#pragma once

#include <string>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <ctime>
#include <cstddef>
#include <cstdint>

struct Channel 
{
    std::string name;

    std::unordered_set<int> members;
    std::unordered_set<int> operators;
    std::unordered_set<int> voiced;
    std::unordered_set<int> banned; 

    bool inviteOnly = false;
    bool topicRestricted = false;
    bool moderated = false;

    std::string key;
    std::size_t userLimit = 0;

    std::string topic;
    std::string topicSetter;
    std::time_t topicTime = 0;

    std::set<std::string>   banMasks;
    std::unordered_set<int> invited;

    std::unordered_map<int, std::uint64_t> joinOrder; // for channel op succession
    std::uint64_t nextJoinSeq = 0;
};