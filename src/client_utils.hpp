#pragma once
#include "protocol/protocol.hpp"

#include "instrumentation_client.hpp"

namespace client_utils {
template <typename T>
inline auto run(const T& cmd,
                instrumentation_client::InstrumentationClient* client) {
  try {
    auto r = client->run_one(cmd);
    DPRINTF("res=%s\n", to_json(r).c_str());
    return yrclient::from_any<T>(r.result()).result();
  } catch (const std::exception& e) {
    DPRINTF("failed to run: %s\n", e.what());
    throw;
  }
}
}  // namespace client_utils
