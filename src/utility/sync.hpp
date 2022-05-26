#pragma once
#include <tuple>
#include <mutex>
namespace util {
/// Obtain exclusive access to resource @v guarded by mutex @m
template <typename T>
inline auto acquire(std::mutex& m, T* v) { // NOLINT
  return std::make_tuple(std::unique_lock<std::mutex>(m), v);
}
}  // namespace util
