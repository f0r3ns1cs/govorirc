#include "Utils.h"
#include "config.h"

#include <algorithm>
#include <cstddef>

static bool isNickSpecial(unsigned char c) noexcept 
{
    switch (c) {
    case '[': case ']': case '\\': case '`':
    case '_': case '^': case '{': case '|': case '}':
        return true;
    default:
        return false;
    }
}

static bool isAlpha(unsigned char c) noexcept 
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool isDigit(unsigned char c) noexcept
{
    return c >= '0' && c <= '9';
}

static bool isAlnum(unsigned char c) noexcept 
{
    return isAlpha(c) || isDigit(c);
}

static bool isControlByte(char c) noexcept {
    return c == '\0' || c == '\r' || c == '\n';
}

std::vector<std::string> Utils::split(std::string_view s, char delim) 
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t end = s.find(delim, start);
        if (end == std::string_view::npos) {
            if (start < s.size())
                out.emplace_back(s.substr(start));
            break;
        }
        if (end > start)
            out.emplace_back(s.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

bool Utils::globMatch(std::string_view pattern, std::string_view text) 
{
    const std::string p = ircCaseFold(pattern);
    const std::string t = ircCaseFold(text);

    size_t pi = 0, ti = 0;
    size_t star = std::string::npos;
    size_t match = 0;

    while (ti < t.size()) {
        if (pi < p.size() && (p[pi] == '?' || p[pi] == t[ti])) {
            ++pi;
            ++ti;
        } 
        else if (pi < p.size() && p[pi] == '*') {
            star = pi++;
            match = ti;
        } 
        else if (star != std::string::npos) {
            pi = star + 1;
            ti = ++match;
        } 
        else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == '*')
        ++pi;
    return pi == p.size();
}

std::string Utils::ircCaseFold(std::string_view s) 
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out.push_back(static_cast<char>(ircFoldChar(c)));
    return out;
}

bool Utils::sameNick(std::string_view a, std::string_view b) noexcept 
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (ircFoldChar(static_cast<unsigned char>(a[i])) !=
            ircFoldChar(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

bool Utils::isValidNick(std::string_view nick) noexcept 
{
    if (nick.empty() || nick.size() > MAX_NICKLEN)
        return false;

    if (!isAlpha(static_cast<unsigned char>(nick[0])) &&
        !isNickSpecial(static_cast<unsigned char>(nick[0])))
        return false;

    for (size_t i = 1; i < nick.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(nick[i]);
        if (!isAlnum(ch) && !isNickSpecial(ch) && ch != '-')
            return false;
    }
    return true;
}

bool Utils::isValidUsername(std::string_view u) noexcept 
{
    if (u.empty() || u.size() > MAX_USERLEN)
        return false;

    for (char ch : u) {
        if (isControlByte(ch) || ch == ' ' || ch == '@' || ch == '!' || ch == ':')
            return false;
    }
    return true;
}

bool Utils::isChannelName(std::string_view t) noexcept 
{
    return !t.empty() && (t[0] == '#' || t[0] == '&');
}

bool Utils::isValidChannelName(std::string_view ch) noexcept 
{
    if (ch.size() < 2 || ch.size() > MAX_CHANNELLEN)
        return false;
    if (ch[0] != '#' && ch[0] != '&')
        return false;
    for (char c : ch) {
        if (isControlByte(c) || c == ' ' || c == ',' || c == '\a')
            return false;
    }
    return true;
}

std::string Utils::toUpper(std::string_view sv) 
{
    std::string s(sv);
    for (char& ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 'a' && c <= 'z')
            ch = static_cast<char>(c - ('a' - 'A'));
    }
    return s;
}

void Utils::stripControlCharsInPlace(std::string& s) noexcept 
{
    for (char& ch : s) {
        if (isControlByte(ch))
            ch = ' ';
    }
}

std::string Utils::stripControlChars(std::string s) 
{
    stripControlCharsInPlace(s);
    return s;
}

std::string Utils::stripCRLF(std::string t, size_t max) 
{
    stripControlCharsInPlace(t);
    if (t.size() > max)
        t.resize(max);
    return t;
}

bool Utils::constantTimeEquals(std::string_view a, std::string_view b) noexcept 
{
    const size_t n = std::max(a.size(), b.size());
    // volatile so the accumulation can't be optimized into an early-out compare.
    volatile unsigned char acc = static_cast<unsigned char>(a.size() ^ b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char av = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char bv = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        acc = static_cast<unsigned char>(acc | (av ^ bv));
    }
    return acc == 0;
}