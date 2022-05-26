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

// Periodically call fn until it returns true or timeout occurs
template <typename Pred>
inline void call_until(const std::chrono::milliseconds timeout,
                       const std::chrono::milliseconds rate, Pred fn) {
  auto deadline = util::current_time() + timeout;
  while (fn() && util::current_time() < deadline) {
    util::sleep_ms(rate);
  }
}

}  // namespace util
