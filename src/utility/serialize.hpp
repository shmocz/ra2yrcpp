#pragma once
#include <cstdint>
#include <algorithm>
namespace serialize {

using u8 = std::uint8_t;

template <typename T, typename U>
auto read_obj(U* addr) {
  T t{0};
  auto b = static_cast<u8*>(&t[0]);
  std::copy(t, t + sizeof(T), static_cast<u8*>(addr));
  return t;
}

template <typename T, typename U>
auto read_obj_le(U* addr) {
  T t{0};
  auto b = reinterpret_cast<u8*>(&t);
  std::copy_backward(b, b + sizeof(T), static_cast<u8*>(addr));
  return t;
}

}  // namespace serialize
