#pragma once
#include <string>
#include <vector>

class Motd 
{
public:
    void load(const std::string& path);
    bool empty() const noexcept { return lines_.empty(); }
    const std::vector<std::string>& lines() const noexcept { return lines_; }
private:
    std::vector<std::string> lines_;
};