#pragma once

#include "CommandRegistry.h"

class CommandWho final : public Command
{
public:
	CommandWho() : Command("WHO", 1, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

