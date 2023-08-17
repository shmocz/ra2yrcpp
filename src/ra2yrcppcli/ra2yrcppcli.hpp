#pragma once

#include "ra2yrproto/core.pb.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace multi_client {
class AutoPollClient;
}

namespace ra2yrcppcli {

constexpr std::array<const char*, 2> INIT_COMMANDS{
    "ra2yrproto.commands.CreateHooks", "ra2yrproto.commands.CreateCallbacks"};

ra2yrproto::Response send_command(multi_client::AutoPollClient* A,
                                  const std::string name,
                                  const std::string args = "");

std::map<std::string, std::string> parse_kwargs(
    std::vector<std::string> tokens);

};  // namespace ra2yrcppcli
