#pragma once

#include "CommandRegistry.h"

class CommandNick final : public Command
{
public:
	CommandNick() : Command("NICK", 1, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


