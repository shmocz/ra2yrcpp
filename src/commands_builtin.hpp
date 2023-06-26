#pragma once

#include "command/command.hpp"

#include <map>
#include <string>

namespace yrclient {
namespace commands_builtin {

std::map<std::string, command::Command::handler_t> get_commands();

}  // namespace commands_builtin
}  // namespace yrclient
