#pragma once

#include <fmt/core.h>
#include <fmt/std.h>

#include <cstdint>
#include <cstdio>

#include <array>
#include <chrono>
#include <thread>

namespace ra2yrcpp {
namespace logging {

enum class Level : int { ERROR = 0, DEBUG = 1, WARNING = 2, INFO = 3 };

constexpr std::array<const char*, 4> levels = {"ERROR", "DEBUG", "WARNING",
                                               "INFO"};

template <typename... Args>
inline void print_message(FILE* fp, const Level level, const char* fmt_s,
                          const char* file, const char* func, const int line,
                          Args... args) {
  fmt::print(
      fp, "{}: [thread {} TS: {}]: {}:{}:{} {}\n",
      levels[static_cast<int>(level)],
      std::hash<std::thread::id>{}(std::this_thread::get_id()),
      static_cast<std::uint64_t>(
          std::chrono::high_resolution_clock::now().time_since_epoch().count()),
      file, func, line, fmt::format(fmt_s, args...));
}

template <typename... Args>
inline void debug(const char* s, const char* file, const char* func,
                  const int line, Args... args) {
  print_message(stderr, Level::DEBUG, s, file, func, line, args...);
}

template <typename... Args>
inline void eerror(Args... args) {
  print_message(stderr, Level::ERROR, args...);
}

}  // namespace logging
}  // namespace ra2yrcpp

#define VA_ARGS(...) , ##__VA_ARGS__
#define LOCATION_INFO() __FILE__, __func__, __LINE__

#ifndef NDEBUG
#define dprintf(fmt, ...)                                                \
  do {                                                                   \
    ra2yrcpp::logging::debug(fmt, LOCATION_INFO() VA_ARGS(__VA_ARGS__)); \
  } while (0)
#else
#define dprintf(...)
#endif

#define eprintf(fmt, ...)                                                 \
  do {                                                                    \
    ra2yrcpp::logging::eerror(fmt, LOCATION_INFO() VA_ARGS(__VA_ARGS__)); \
  } while (0)
