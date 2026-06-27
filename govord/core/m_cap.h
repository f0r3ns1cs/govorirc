#pragma once

#include "CommandRegistry.h"

class CommandCap final : public Command
{
public:
	CommandCap() : Command("CAP", 1, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


