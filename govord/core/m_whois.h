#pragma once

#include "CommandRegistry.h"

class CommandWhois final : public Command
{
public:
	CommandWhois() : Command("WHOIS", 1, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};

