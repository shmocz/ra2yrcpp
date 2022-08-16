#pragma once
#include "protocol/protocol.hpp"

#include "instrumentation_client.hpp"

namespace client_utils {
template <typename T>
inline auto run(const T& cmd,
                instrumentation_client::InstrumentationClient* client) {
  try {
    auto r = client->run_one(cmd);
    dprintf("res={}", to_json(r).c_str());
    return yrclient::from_any<T>(r.result()).result();
  } catch (const std::exception& e) {
    dprintf("failed to run: {}", e.what());
    throw;
  }
}
}  // namespace client_utils
