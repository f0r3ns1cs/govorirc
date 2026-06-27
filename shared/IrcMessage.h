#pragma once

#include <string>
#include <vector>
#include <optional>


struct Tag 
{
	std::string k, v;
};
struct Prefix 
{
	std::string nick, user, host;
	bool isServer = false;
};
struct Message 
{
	std::optional<std::vector<Tag>> tags;
	std::optional<Prefix> prefix;
	std::string command;
	std::vector<std::string> params;
	bool valid = false;
};

Message tokenizeMsg(std::string_view msg);
std::string serializeMsg(const Message& msg);
std::string formatOutbound(std::string_view input, std::string_view currentTarget);
