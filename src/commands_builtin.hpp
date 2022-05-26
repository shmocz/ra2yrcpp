#pragma once

#include "command/command.hpp"
#include "instrumentation_service.hpp"
#include "protocol/protocol.hpp"
#include "util_string.hpp"
#include "utility.h"
#include "util_command.hpp"
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace commands_builtin {

using yrclient::InstrumentationService;

void command_deleter(command::Command* c);
std::map<std::string, command::Command::handler_t>* get_commands();

}  // namespace commands_builtin
