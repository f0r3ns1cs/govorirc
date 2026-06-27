#pragma once

#include "CommandRegistry.h"

class CommandPing final : public Command
{
public:
	CommandPing() : Command("PING", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

class CommandPong final : public Command
{
public:
	CommandPong() : Command("PONG", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


