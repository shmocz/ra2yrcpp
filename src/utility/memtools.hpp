#pragma once
#include <cstdint>

#include <memory>

namespace utility {
template <typename T, typename... Args>
std::unique_ptr<void, void (*)(void*)> make_uptr(Args... args) {
  return std::unique_ptr<void, void (*)(void*)>(new T(args...), [](auto d) {
    delete reinterpret_cast<T*>(d);  // NOLINT
  });
}

template <typename T, typename... Args>
std::shared_ptr<void> make_sptr(Args... args) {
  return std::shared_ptr<void>(new T(args...),
                               [](auto d) { delete d; });  // NOLINT
}

template <typename T = std::uintptr_t, typename S = void*>
auto asint(const S p) {
  return reinterpret_cast<T>(p);  // NOLINT
}

template <typename T = void*>
T asptr(const std::uintptr_t p) {
  return reinterpret_cast<T>(p);  // NOLINT
}

}  // namespace utility
