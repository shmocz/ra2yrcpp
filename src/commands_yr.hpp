#pragma once
#include "protocol/protocol.hpp"

#include "async_queue.hpp"
#include "command/command.hpp"
#include "errors.hpp"
#include "hook.hpp"
#include "instrumentation_service.hpp"
#include "ra2/abi.hpp"
#include "ra2/abstract_types.hpp"
#include "ra2/game_state.hpp"
#include "ra2/general.h"
#include "ra2/objects.hpp"
#include "ra2/state_parser.hpp"
#include "ra2/type_classes.hpp"
#include "ra2/utility.hpp"
#include "util_command.hpp"
#include "util_string.hpp"
#include "utility.h"

#include <algorithm>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/repeated_ptr_field.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace commands_yr {

using yrclient::InstrumentationService;

std::map<std::string, command::Command::handler_t>* get_commands();

}  // namespace commands_yr
