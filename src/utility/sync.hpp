#pragma once
#include <mutex>
#include <tuple>
#include <utility>

namespace util {
/// Obtain exclusive access to resource @v guarded by mutex @m
template <typename T, typename MutexT = std::mutex>
inline auto acquire(MutexT& m, T* v) {  // NOLINT
  return std::make_tuple(std::move(std::unique_lock<MutexT>(m)), v);
}
}  // namespace util
