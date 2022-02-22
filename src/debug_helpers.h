#pragma once
#ifndef NDEBUG
#include <stdio.h>
#include <thread>
#define DPRINTF(fmt, args...)                                         \
  do {                                                                \
    fprintf(stderr, "DEBUG: [thread %u] %s:%d:%s(): " fmt,            \
            std::hash<std::thread::id>{}(std::this_thread::get_id()), \
            __FILE__, __LINE__, __func__, ##args);                    \
  } while (0)
#else
#define DPRINTF(...)
#endif