#pragma once

#include <string>

namespace Cli 
{
    std::string trim(std::string s);

    std::string prompt(const std::string& label, const std::string& def = {});
    std::string promptHidden(const std::string& label);

    std::string sanitizeUsername(std::string u);
    std::string sanitizeRealname(std::string r);

    std::string envUser();
    std::string defaultNick();
    std::string defaultUser(const std::string& nick);
}
