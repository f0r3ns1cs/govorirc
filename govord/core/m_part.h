#pragma once

#include "CommandRegistry.h"

class CommandPart final : public Command
{
public:
	CommandPart() : Command("PART", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

