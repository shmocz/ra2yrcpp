#pragma once
#include "types.h"
#include <string>
#include <vector>

namespace yrclient {
inline vecu8 to_bytes(std::string msg) { return vecu8(msg.begin(), msg.end()); }
inline std::string to_string(const vecu8& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

// split string by delimiter regex and return vector of strings
std::vector<std::string> split_string(const std::string& s,
                                      const std::string delim = "[\\s]+");
}  // namespace yrclient
