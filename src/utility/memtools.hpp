#pragma once

#include <memory>

namespace utility {
template <typename T, typename... Args>
std::unique_ptr<void, void (*)(void*)> make_uptr(Args... args) {
  return std::unique_ptr<void, void (*)(void*)>(new T(args...), [](auto* d) {
    delete reinterpret_cast<T*>(d);  // NOLINT
  });
}

}  // namespace utility
