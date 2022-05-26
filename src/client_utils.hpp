#pragma once
#include "instrumentation_client.hpp"
#include <type_traits>

namespace client_utils {
template <typename T>
inline auto run(const T& cmd,
                instrumentation_client::InstrumentationClient* client) {
  std::remove_cv_t<std::remove_reference_t<decltype(cmd)>> res;
  auto r = client->run_one(cmd);
  r.body().UnpackTo(&res);
  return res.result();
}
}  // namespace client_utils
