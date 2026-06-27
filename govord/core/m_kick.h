#pragma once

#include "CommandRegistry.h"

class CommandKick final : public Command
{
public:
	CommandKick() : Command("KICK", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


