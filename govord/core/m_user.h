#pragma once

#include "CommandRegistry.h"

class CommandUser final : public Command
{
public:
	CommandUser() : Command("USER", 4, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

