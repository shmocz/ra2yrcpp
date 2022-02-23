#pragma once
#include <functional>

// Adapted from https://stackoverflow.com/questions/10270328/the-simplest-and-neatest-c11-scopeguard
namespace utility {
class scope_guard {
 private:
  std::function<void()> _f;

 public:
  template <typename FnT>
  scope_guard(FnT&& f) : _f(std::forward<FnT>(f)) {}
  scope_guard(scope_guard&& o) : _f(std::move(o._f)) { o._f = nullptr; }

  ~scope_guard() {
    if (_f) {
      _f();
    }
  }
  void operator=(const scope_guard&) = delete;
};
}  // namespace utility
