#pragma once

#include "CommandRegistry.h"

class CommandNames final : public Command
{
public:
	CommandNames() : Command("NAMES", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

