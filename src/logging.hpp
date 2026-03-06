#pragma once

#include <fmt/core.h>
#include <fmt/std.h>

#include <cstdint>
#include <cstdio>

#include <array>
#include <chrono>
#include <thread>

#if !defined(NDEBUG) && !defined(DEBUG_LOG)
#define DEBUG_LOG
#endif

namespace ra2yrcpp {
namespace logging {

enum class Level : int { ERROR = 0, DEBUG = 1, WARNING = 2, INFO = 3 };

constexpr std::array<const char*, 4> levels = {"ERROR", "DEBUG", "WARNING",
                                               "INFO"};

/// Open output log handle if no previous handle is active.
/// @param path Output log path. If nullptr, set up a null handle.
/// @return True if log file was opened succesfully.
bool set_output_handle(const char* path);

/// Get output log handle
/// @return Output file handle. If no handle has been configured, or previous handle has been closed
/// returns stderr.
FILE* get_output_handle();

/// Flush and close a previously opened output handle.
void close_output_handle();

template <typename... Args>
inline void print_message(FILE* fp, Level level, const char* fmt_s,
                          const char* file, const char* func, int line,
                          Args... args) {
  // TODO: Better coding
  void initialize_stderr();
  initialize_stderr();
  // TODO: Better coding
  fmt::print(
      fp, "{}: [thread {} TS: {}]: {}:{}:{} {}\n",
      levels[static_cast<int>(level)],
      std::hash<std::thread::id>{}(std::this_thread::get_id()),
      static_cast<std::uint64_t>(
          std::chrono::high_resolution_clock::now().time_since_epoch().count()),
      file, func, line, fmt::format(fmt_s, args...));
}

template <typename... Args>
inline void print_error(Level level, const char* fmt_s, const char* file,
                        const char* func, int line, Args... args) {
  auto* fp = get_output_handle();
  if (fp == nullptr) {
    return;
  }
  print_message(fp, level, fmt_s, file, func, line, args...);
}

template <typename... Args>
inline void debug(const char* s, const char* file, const char* func,
                  const int line, Args... args) {
  print_error(Level::DEBUG, s, file, func, line, args...);
}

template <typename... Args>
inline void eerror(Args... args) {
  print_error(Level::ERROR, args...);
}

}  // namespace logging
}  // namespace ra2yrcpp

#define VA_ARGS(...) , ##__VA_ARGS__
#define LOCATION_INFO() __FILE__, __func__, __LINE__

#ifdef DEBUG_LOG
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

#define wrprintf(fmt, ...)                                                 \
  do {                                                                     \
    ra2yrcpp::logging::print_error(ra2yrcpp::logging::Level::WARNING, fmt, \
                                   LOCATION_INFO() VA_ARGS(__VA_ARGS__));  \
  } while (0)

#define iprintf(fmt, ...)                                                 \
  do {                                                                    \
    ra2yrcpp::logging::print_error(ra2yrcpp::logging::Level::INFO, fmt,   \
                                   LOCATION_INFO() VA_ARGS(__VA_ARGS__)); \
  } while (0)
