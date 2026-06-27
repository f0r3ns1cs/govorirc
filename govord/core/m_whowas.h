#pragma once

#include "CommandRegistry.h"

class CommandWhowas final : public Command
{
public:
	CommandWhowas() : Command("WHOWAS", 1, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

