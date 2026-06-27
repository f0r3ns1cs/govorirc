#pragma once

#include "CommandRegistry.h"

class CommandNotice final : public Command
{
public:
	CommandNotice() : Command("NOTICE", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

