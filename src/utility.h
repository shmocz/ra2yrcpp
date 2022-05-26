#pragma once
#include <map>

namespace yrclient {
inline int divup(int a, int b) { return (a + b - 1) / b; }
/// Returns true if @value is in @container. Otherwise false.
template <typename T, typename V>
inline bool contains(const T& container, const V& value) {
  return find(container.begin(), container.end(), value) != container.end();
}

/// Returns true if key @value is in the map @m. Otherwise false.
template <typename K, typename V>
inline bool contains(const std::map<K, V>& m, const V& value) {
  return m.find(value) != m.end();
}

template <typename T, typename U>
inline T* asptr(U u) {
  return reinterpret_cast<T*>(u);
}

template <typename T, typename U>
inline T as(U u) {
  return reinterpret_cast<T>(u);
}

}  // namespace yrclient
