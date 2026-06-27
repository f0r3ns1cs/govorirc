#pragma once

#include "CommandRegistry.h"

class CommandMotd final : public Command
{
public:
	CommandMotd() : Command("MOTD", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


