#pragma once
#include "command_manager.hpp"
#include "instrumentation_service.hpp"
#include <map>
#include <memory>
#include <string>

namespace commands_yr {

using command_manager::CommandResult;
using yrclient::InstrumentationService;

std::map<std::string, yrclient::IServiceCommand>* get_commands();

}  // namespace commands_yr
