#pragma once

#include "types.h"

#include <xbyak/xbyak.h>

#include <cstddef>
#undef ERROR
#undef OK

namespace hook {

#pragma pack(push, 16)

struct HookEntry {
  u32 address;
  u32 size;
  const char* name;
};

#pragma pack(pop)

using hook_fn = u32 __cdecl (*)(X86Regs*);

/// In Syringe hook format, the overwritten instruction is executed after hook.
/// NB: We don't need to obey syringe prologue rigorously. As long export the
/// hook functions in Syringe compatible format, and ensure that REGISTER
/// argument is passed correctly, we should be fine. Only when ra2yrcpp is
/// executed without Syringe (e.g.) with custom loader code, the internal
/// prologue format should be used.
struct SyringeHook : Xbyak::CodeGenerator {
  SyringeHook(addr_t target, addr_t hook_function, std::size_t code_length);
};

class Hook {
 public:
  Hook(HookEntry h, hook_fn fn);

 private:
  SyringeHook trampoline_;
};

}  // namespace hook

#ifdef _MSC_VER
#define HOOK_SECTION_ENTRY(hook, funcname, size) \
  __declspec(allocate(".syhks00"))               \
      HookEntry _hk_##hook##funcname = {hook, size, #funcname}
#else
#define HOOK_SECTION_ENTRY(hook, funcname, size) \
  HookEntry __attribute__((section(".syhks00"))) \
  _hk_##hook##funcname = {hook, size, #funcname}
#endif

// NB. Syringe headers specify the HOOK_SECTION_ENTRY inside SyringeData::Hooks
// namespace.
#define DEFINE_HOOK(hook, funcname, size)   \
  HOOK_SECTION_ENTRY(hook, funcname, size); \
  extern "C" __declspec(dllexport) DWORD __cdecl funcname(void* R)
