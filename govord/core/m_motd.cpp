#include "m_motd.h"
#include "IrcMessage.h"
#include "../Server.h"

void CommandMotd::execute(Server& s, Client& c, const Message& msg) const
{
	// MOTD
	// MOTD <server>
	(void)msg;

	s.sendMotd(c);
}
static AutoRegisterCommand<CommandMotd> s_reg_motd("MOTD");
