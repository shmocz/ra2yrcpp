#pragma once

#include "protocol/protocol.hpp"

#include "config.hpp"
#include "dll_inject.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "multi_client.hpp"
#include "network.hpp"
#include "process.hpp"
#include "utility/time.hpp"

#include <argparse/argparse.hpp>
#include <fmt/core.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ra2yrcppcli {

constexpr std::array<const char*, 2> INIT_COMMANDS{
    "ra2yrproto.commands.CreateHooks", "ra2yrproto.commands.CreateCallbacks"};

ra2yrproto::Response send_command(multi_client::AutoPollClient* A,
                                  const std::string name,
                                  const std::string args = "");

std::map<std::string, std::string> parse_kwargs(
    std::vector<std::string> tokens);

};  // namespace ra2yrcppcli
