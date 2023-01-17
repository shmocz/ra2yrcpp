#pragma once
#include <tuple>

// Adapted from:
// https://devblogs.microsoft.com/oldnewthing/20200713-00/?p=103978

namespace utility {

#define CC_CDECL __cdecl
#define CC_STDCALL __stdcall

struct FunctionCC {
  using Cdecl = void(CC_CDECL*)();      // NOLINT
  using Stdcall = void(CC_STDCALL*)();  // NOLINT
};

template <typename T>
struct FunctionTraits;

#define MAKE_TRAIT(CC, name)                                  \
  template <typename T, typename... Args>                     \
  struct FunctionTraits<T(CC*)(Args...)> {                    \
    using RetType = T;                                        \
    using ArgsT = std::tuple<Args...>;                        \
    using Pointer = T(CC*)(Args...);                          \
    using CallingConvention = FunctionCC::name;               \
    static constexpr auto NumArgs = std::tuple_size_v<ArgsT>; \
  }

MAKE_TRAIT(CC_CDECL, Cdecl);
MAKE_TRAIT(CC_STDCALL, Stdcall);

#undef MAKE_TRAIT
#undef CC_STDCALL
#undef CC_CDECL

}  // namespace utility
