#pragma once

#include "CommandRegistry.h"

class CommandTopic final : public Command
{
public:
	CommandTopic() : Command("TOPIC", 0, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

