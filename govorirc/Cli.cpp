#include "Cli.h"
#include "Utils.h"

#include <cstdlib>
#include <iostream>
#include <print>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

std::string Cli::trim(std::string s) 
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string Cli::prompt(const std::string& label, const std::string& def) 
{
    if (!def.empty())
        std::print("{} [{}]: ", label, def);
    else
        std::print("{}: ", label);
    std::cout.flush();

    std::string line;
    if (!std::getline(std::cin, line))
        return def;
    line = trim(std::move(line));
    return line.empty() ? def : line;
}

std::string Cli::promptHidden(const std::string& label) 
{
    std::print("{} (leave empty for none): ", label);
    std::cout.flush();

    std::string line;

#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool restore = (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode));
    if (restore)
        SetConsoleMode(h, mode & ~ENABLE_ECHO_INPUT);
    std::getline(std::cin, line);
    if (restore)
        SetConsoleMode(h, mode);
#else
    termios oldt{};
    bool restore = (tcgetattr(STDIN_FILENO, &oldt) == 0);
    if (restore) {
        termios newt = oldt;
        newt.c_lflag &= ~ECHO;
        restore = (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == 0);
    }
    std::getline(std::cin, line);
    if (restore)
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

    std::print("\n");
    return trim(std::move(line));
}

std::string Cli::sanitizeUsername(std::string u) 
{
    std::string out;
    out.reserve(u.size());
    for (char ch : u) {
        if (ch == '\0' || ch == '\r' || ch == '\n' ||
            ch == ' ' || ch == '@' || ch == '!' || ch == ':')
            continue;
        out += ch;
    }
    if (out.size() > 16)
        out.resize(16);
    return out;
}

std::string Cli::sanitizeRealname(std::string r) 
{
    for (char& ch : r) {
        if (ch == '\r' || ch == '\n' || ch == '\0')
            ch = ' ';
    }
    if (r.size() > 100)
        r.resize(100);
    return r;
}

std::string Cli::envUser() 
{
    if (const char* e = std::getenv("USER"))
        return e;
    if (const char* e = std::getenv("USERNAME"))
        return e;
    return {};
}

std::string Cli::defaultNick()
{
    std::string u = envUser();
    if (Utils::isValidNick(u))
        return u;

    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> d(1000, 9999);
    return std::format("guest{}", d(g));
}

std::string Cli::defaultUser(const std::string& nick) 
{
    std::string u = sanitizeUsername(envUser());
    return u.empty() ? sanitizeUsername(nick) : u;
}