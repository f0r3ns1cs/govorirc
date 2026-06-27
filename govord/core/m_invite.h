#pragma once

#include "CommandRegistry.h"

class CommandInvite final : public Command
{
public:
	CommandInvite() : Command("INVITE", 2, false) {}
	void execute(Server& s, Client& c, const Message& msg) const override;
};


