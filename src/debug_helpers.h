#pragma once
#ifndef NDEBUG
#define DPRINTF(fmt, args...)                                                \
  do {                                                                       \
    fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, \
            ##args);                                                         \
  } while (0)
#else
#define DPRINTF(...)
#endif