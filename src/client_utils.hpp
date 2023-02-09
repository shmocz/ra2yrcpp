#pragma once
#include "protocol/protocol.hpp"

#include "instrumentation_client.hpp"

namespace client_utils {
//
// Sends a command execution request and reads the command result back.
// @param cmd The command (protobuf Message) to be executed
//
template <typename T>
inline auto run(const T& cmd,
                instrumentation_client::InstrumentationClient* client) {
  try {
    auto r = client->run_one(cmd);
    return yrclient::from_any<T>(r.result()).result();
  } catch (const std::exception& e) {
    dprintf("failed to run: {}", e.what());
    throw;
  }
}
}  // namespace client_utils
