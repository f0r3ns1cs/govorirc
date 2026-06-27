#pragma once

#include "Command.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class CommandRegistry
{
public:
    using Factory = std::function<std::unique_ptr<Command>()>;

    static CommandRegistry& instance()
    {
        static CommandRegistry r;
        return r;
    }

    template<typename T>
    void registerCommand(const std::string& name)
    {
        commands_[name] = [] {
            return std::make_unique<T>();
            };
    }

    std::unique_ptr<Command> create(const std::string& name) const 
    {
        auto it = commands_.find(name);

        if (it == commands_.end()) {
            return nullptr;
        }

        return it->second();
    }

private:
    CommandRegistry() = default;

    std::unordered_map<std::string, Factory> commands_;
};

template<typename T>
class AutoRegisterCommand 
{
public:
    AutoRegisterCommand(const std::string& name) {
        CommandRegistry::instance().registerCommand<T>(name);
    }
};