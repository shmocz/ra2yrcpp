#pragma once
#include <algorithm>
#include <map>

namespace ra2yrcpp {
/// Returns true if @value is in @container. Otherwise false.
template <typename T, typename V>
inline bool contains(const T& container, const V& value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

/// Returns true if key @value is in the map @m. Otherwise false.
template <typename K, typename V>
inline bool contains(const std::map<K, V>& m, const V& value) {
  return m.find(value) != m.end();
}

}  // namespace ra2yrcpp
