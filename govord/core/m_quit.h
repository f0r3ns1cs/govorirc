#pragma once

#include "CommandRegistry.h"

class CommandQuit final : public Command
{
public:
	CommandQuit() : Command("QUIT", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

