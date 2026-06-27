#include "Motd.h"
#include "Utils.h"
#include <fstream>

void Motd::load(const std::string& path) 
{
    lines_.clear();
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        Utils::stripControlCharsInPlace(line);
        if (line.size() > 450) line.resize(450);
        lines_.push_back(std::move(line));
    }
}