#pragma once
#ifndef NDEBUG
#include <cstdint>

#include <chrono>
#include <stdio.h>
#include <thread>
#define DPRINTF(fmt, args...)                                                \
  do {                                                                       \
    fprintf(                                                                 \
        stderr, "DEBUG: [thread %u] TS: %llu %s:%d:%s(): " fmt,              \
        std::hash<std::thread::id>{}(std::this_thread::get_id()),            \
        static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now() \
                                       .time_since_epoch()                   \
                                       .count()),                            \
        __FILE__, __LINE__, __func__, ##args);                               \
  } while (0)

#else
#define DPRINTF(...)
#endif
