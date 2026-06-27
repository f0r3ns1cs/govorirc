#pragma once

#include "CommandRegistry.h"

class CommandMode final : public Command
{
public:
	CommandMode() : Command("MODE", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


