#pragma once
#include "types.h"
#include <string>

namespace yrclient {
inline vecu8 to_bytes(std::string msg) { return vecu8(msg.begin(), msg.end()); }
inline std::string to_string(const vecu8& bytes) {
  return std::string(bytes.begin(), bytes.end());
}
}  // namespace yrclient
