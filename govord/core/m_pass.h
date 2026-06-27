#pragma once

#include "CommandRegistry.h"

class CommandPass final : public Command
{
public:
	CommandPass() : Command("PASS", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

