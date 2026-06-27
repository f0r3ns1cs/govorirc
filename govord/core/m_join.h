#pragma once

#include "CommandRegistry.h"

class CommandJoin final : public Command
{
public:
	CommandJoin() : Command("JOIN", 1, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


