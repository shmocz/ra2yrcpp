#pragma once

#include "protocol/protocol.hpp"

#include "config.hpp"
#include "dll_inject.hpp"
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

struct IServiceOptions {
  unsigned max_clients;
  unsigned port;
  std::string host;
  IServiceOptions() : max_clients(0u), port(0u) {}
};

struct DLLInjectOptions {
  unsigned delay_pre;
  unsigned delay_post;
  unsigned wait_process;
  std::string process_name;
  bool force;
  DLLInjectOptions()
      : delay_pre(0u),
        delay_post(0u),
        wait_process(0u),
        process_name(""),
        force(false) {}
};

constexpr std::array<const char*, 2> INIT_COMMANDS{
    "yrclient.commands.CreateHooks", "yrclient.commands.CreateCallbacks"};

void inject_dll(unsigned pid, const std::string path_dll,
                IServiceOptions options, DLLInjectOptions dll);

yrclient::Response send_command(multi_client::AutoPollClient* A,
                                const std::string name,
                                const std::string args = "");

std::map<std::string, std::string> parse_kwargs(
    std::vector<std::string> tokens);

};  // namespace ra2yrcppcli
