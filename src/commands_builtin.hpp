#pragma once

#include "protocol/protocol.hpp"

#include "command/command.hpp"
#include "instrumentation_service.hpp"
#include "util_command.hpp"
#include "util_string.hpp"
#include "utility.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace yrclient {
namespace commands_builtin {

using yrclient::InstrumentationService;

void command_deleter(command::Command* c);
std::map<std::string, command::Command::handler_t>* get_commands();

}  // namespace commands_builtin
}  // namespace yrclient
