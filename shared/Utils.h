#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

namespace Utils 
{
    std::vector<std::string> split(std::string_view s, char delim);
    inline std::vector<std::string> splitCsv(std::string_view s) { return split(s, ','); }

    bool globMatch(std::string_view pattern, std::string_view text);

    constexpr unsigned char ircFoldChar(unsigned char c) noexcept 
    {
        switch (c) {
        case '[':  return '{';
        case ']':  return '}';
        case '\\': return '|';
        case '^':  return '~';
        default:
            return (c >= 'A' && c <= 'Z')
                ? static_cast<unsigned char>(c + ('a' - 'A'))
                : c;
        }
    }

    std::string ircCaseFold(std::string_view s);
    bool sameNick(std::string_view a, std::string_view b) noexcept;

    bool isValidNick(std::string_view nick) noexcept;
    bool isValidUsername(std::string_view u) noexcept;
    bool isValidChannelName(std::string_view ch) noexcept;
    bool isChannelName(std::string_view t) noexcept;

    std::string toUpper(std::string_view sv);

    void stripControlCharsInPlace(std::string& s) noexcept;
    std::string stripControlChars(std::string s);
    std::string stripCRLF(std::string t, std::size_t max);

    bool constantTimeEquals(std::string_view a, std::string_view b) noexcept;
}
