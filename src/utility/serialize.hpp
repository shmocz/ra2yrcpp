#pragma once
#include <cstdint>

#include <algorithm>
namespace serialize {

using u8 = std::uint8_t;

template <typename T, typename U>
auto read_obj(U* addr) {
  T t{0};
  auto a = reinterpret_cast<const u8*>(addr);
  auto b = reinterpret_cast<u8*>(&t);
  std::copy(a, a + sizeof(T), b);
  return t;
}

template <typename T, typename U>
auto read_obj_le(const U* addr) {
  T t{0};
  auto b = reinterpret_cast<u8*>(&t);
  auto a = static_cast<const u8*>(addr);
  std::copy_backward(a, a + sizeof(T), b);
  return t;
}

}  // namespace serialize
