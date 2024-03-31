#pragma once
#include "types.h"

#include <chrono>
#include <thread>

namespace util {
inline std::chrono::system_clock::time_point current_time() {
  return std::chrono::system_clock::now();
}

template <typename T>
inline void sleep_ms(T s) {
  std::this_thread::sleep_for(std::chrono::milliseconds(s));
}

template <typename T, typename R>
inline void sleep_ms(std::chrono::duration<T, R> s) {
  std::this_thread::sleep_for(std::chrono::duration_cast<duration_t>(s));
}

// Periodically call fn until it returns true or timeout occurs
template <typename Pred>
inline void call_until(duration_t timeout, duration_t rate, Pred fn) {
  auto deadline = util::current_time() + timeout;
  while (fn() && util::current_time() < deadline) {
    util::sleep_ms(rate);
  }
}

}  // namespace util
