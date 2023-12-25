#pragma once
#include <cstdint>
#include <cstring>

#include <algorithm>

namespace serialize {

using u8 = std::uint8_t;

template <typename T, typename U>
auto read_obj(const U* addr) {
  T t;
  std::memset(&t, 0, sizeof(t));
  auto a = reinterpret_cast<const u8*>(addr);
  auto b = reinterpret_cast<u8*>(&t);
  std::copy(a, a + sizeof(T), b);
  return t;
}

template <typename T>
auto read_obj(const std::uintptr_t addr) {
  return read_obj<T>(reinterpret_cast<void*>(addr));
}

template <typename T, typename U>
auto read_obj_le(const U* addr) {
  T t{0};
  auto* b = reinterpret_cast<u8*>(&t);
  auto* a = reinterpret_cast<const u8*>(addr);
  std::copy_backward(a, a + sizeof(T), b + sizeof(T));
  return t;
}

template <typename T>
auto read_obj_le(const std::uintptr_t addr) {
  return read_obj_le<T>(reinterpret_cast<void*>(addr));
}

inline bool bytes_equal(const void* p1, const void* p2, unsigned size) {
#if 0
  for (auto i = 0U; i < N; i++) {
    if (reinterpret_cast<const char*>(p1)[i] !=
        reinterpret_cast<const char*>(p2)[i]) {
      return false;
    }
  }
  return true;
#else
  return std::memcmp(p1, p2, size) == 0;
#endif
}

template <typename T>
bool bytes_equal(const T* p1, const T* p2) {
  return bytes_equal(p1, p2, sizeof(T));
}

}  // namespace serialize
