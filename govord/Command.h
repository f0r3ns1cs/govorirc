#pragma once

#include <string>

#include "fwd.h"

class Command
{
private:
	std::string name_;
	std::size_t minParams_;
	bool        requiresRegistration_;

protected:
	Command(const Command&) = delete;
	Command& operator=(const Command&) = delete;

public:
	Command(std::string_view name,
		std::size_t minParams,
		bool requiresRegistration)
		: name_(name)
		, minParams_(minParams)
		, requiresRegistration_(requiresRegistration)
	{}

	virtual ~Command() = default;

	virtual void execute(Server& srv,
		Client& client,
		const Message& msg) const = 0;

	const std::string& name() const noexcept { return name_; }
	std::size_t        minParams() const noexcept { return minParams_; }
	bool               requiresRegistration() const noexcept { return requiresRegistration_; }
		
};