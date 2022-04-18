#pragma once

namespace yrclient {
inline int divup(int a, int b) { return (a + b - 1) / b; }
/// Returns true if @value is in @container. Otherwise false.
template <typename T, typename V>
bool contains(const T& container, const V& value) {
  return find(container.begin(), container.end(), value) != container.end();
}
}  // namespace yrclient
