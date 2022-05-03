#pragma once

#include "command_manager.hpp"
#include "instrumentation_service.hpp"
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace commands_builtin {

using command_manager::CommandResult;
using yrclient::InstrumentationService;

std::map<std::string, yrclient::IServiceCommand>* get_commands();

}  // namespace commands_builtin
