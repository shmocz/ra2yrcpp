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
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ra2yrcppcli {
struct IServiceOptions {
  unsigned max_clients;
  unsigned port;
  std::string host;
  IServiceOptions() {}
};

struct DLLInjectOptions {
  unsigned delay_pre;
  unsigned delay_post;
  unsigned wait_process;
  std::string process_name;
  float force;
  DLLInjectOptions() {}
};

};  // namespace ra2yrcppcli
