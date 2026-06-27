#pragma once

#include "CommandRegistry.h"

class CommandList final : public Command
{
public:
	CommandList() : Command("LIST", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


