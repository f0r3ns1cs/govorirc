#include "m_pass.h"
#include "IrcMessage.h"
#include "../Server.h"

void CommandPass::execute(Server& s, Client& c, const Message& msg) const
{
    // PASS <password>

    if (msg.params.empty()) {
        s.sendNumeric(c, 461, "PASS :Not enough parameters");
        return;
    }

    if (c.registered) {
        s.sendNumeric(c, 462, ":You may not reregister");
        return;
    }

    c.suppliedPass = msg.params[0];
}
static AutoRegisterCommand<CommandPass> s_reg_pass("PASS");
