#pragma once
#include <chrono>

namespace util {
inline std::chrono::system_clock::time_point current_time() {
  return std::chrono::system_clock::now();
}
}  // namespace util
