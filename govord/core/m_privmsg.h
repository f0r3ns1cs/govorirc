#pragma once

#include "CommandRegistry.h"

class CommandPrivmsg final : public Command
{
public:
	CommandPrivmsg() : Command("PRIVMSG", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

