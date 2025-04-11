#include "logging.hpp"

#include <cstdio>

#include <mutex>

static FILE* g_handle{nullptr};
static std::mutex g_output_handle_mutex;
static bool g_handle_open{false};

bool ra2yrcpp::logging::set_output_handle(const char* path) {
  std::lock_guard<std::mutex> lock(g_output_handle_mutex);
  if (g_handle_open) {
    return false;
  }
  if (g_handle == nullptr) {
    if (path != nullptr && (g_handle = std::fopen(path, "w")) == nullptr) {
      return false;
    }
    g_handle_open = true;
  }
  return true;
}

FILE* ra2yrcpp::logging::get_output_handle() {
  std::lock_guard<std::mutex> lock(g_output_handle_mutex);
  if (!g_handle_open) {
    return stderr;
  }
  return g_handle;
}

void ra2yrcpp::logging::close_output_handle() {
  std::lock_guard<std::mutex> lock(g_output_handle_mutex);
  if (!g_handle_open) {
    return;
  }
  if (g_handle != nullptr) {
    (void)std::fflush(g_handle);
    (void)std::fclose(g_handle);
  }
  g_handle = nullptr;
  g_handle_open = false;
}
