#pragma once
#include "command/is_command.hpp"

#include <map>
#include <string>

namespace commands_yr {

std::map<std::string, ra2yrcpp::command::iservice_cmd::handler_t>
get_commands();

}  // namespace commands_yr
