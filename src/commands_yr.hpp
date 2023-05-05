#pragma once
#include "command/command.hpp"

#include <map>
#include <string>

namespace commands_yr {

std::map<std::string, command::Command::handler_t> get_commands();

}  // namespace commands_yr
