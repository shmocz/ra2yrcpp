#pragma once
#include "command/is_command.hpp"

#include <map>
#include <string>

namespace ra2yrcpp {

namespace commands_game {

std::map<std::string, ra2yrcpp::command::iservice_cmd::handler_t>
get_commands();

}  // namespace commands_game

}  // namespace ra2yrcpp
