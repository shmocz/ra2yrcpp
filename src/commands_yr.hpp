#pragma once
#include "command/command.hpp"
#include "command/command_manager.hpp"
#include "errors.hpp"
#include "instrumentation_service.hpp"
#include "protocol/protocol.hpp"
#include "util_command.hpp"
#include "util_string.hpp"
#include "utility.h"
#include "utility/serialize.hpp"
#include <map>
#include <memory>
#include <string>

namespace commands_yr {

using yrclient::InstrumentationService;

std::map<std::string, command::Command::handler_t>* get_commands();

}  // namespace commands_yr
