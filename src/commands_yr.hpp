#pragma once
#include "protocol/protocol.hpp"

#include "command/command.hpp"
#include "command/command_manager.hpp"
#include "errors.hpp"
#include "google/protobuf/repeated_ptr_field.h"
#include "instrumentation_service.hpp"
#include "ra2/abstract_types.hpp"
#include "ra2/game_state.hpp"
#include "ra2/general.h"
#include "ra2/objects.hpp"
#include "ra2/state_parser.hpp"
#include "ra2/type_classes.hpp"
#include "util_command.hpp"
#include "util_string.hpp"
#include "utility.h"
#include "utility/serialize.hpp"
#include "utility/time.hpp"

#include <fmt/chrono.h>

#include <map>
#include <memory>
#include <string>

namespace commands_yr {

using yrclient::InstrumentationService;

std::map<std::string, command::Command::handler_t>* get_commands();

}  // namespace commands_yr
