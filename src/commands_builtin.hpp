#pragma once

#include "command/is_command.hpp"

#include <map>
#include <string>

namespace yrclient {
namespace commands_builtin {

std::map<std::string, ra2yrcpp::command::iservice_cmd::handler_t>
get_commands();

}  // namespace commands_builtin
}  // namespace yrclient
