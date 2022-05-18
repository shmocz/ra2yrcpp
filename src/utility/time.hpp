#pragma once
#include <chrono>
#include <thread>

namespace util {
inline std::chrono::system_clock::time_point current_time() {
  return std::chrono::system_clock::now();
}

template <typename T>
inline void sleep_ms(const T s) {
    std::this_thread::sleep_for(std::chrono::milliseconds(s));
}
}  // namespace util
