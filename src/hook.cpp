#include "hook.hpp"

#include "logging.hpp"
#include "process.hpp"
#include "x86.hpp"

#include <cstdint>

using namespace hook;

static void patch_code(u8* target_address, const u8* code,
                       std::size_t code_length) {
  dprintf("address={}, bytes={}", reinterpret_cast<void*>(target_address),
          code_length);
  auto P = process::get_current_process();
  P.write_memory(target_address, code, code_length);
}

// This differs from Syringe, which uses relative call
SyringeHook::SyringeHook(addr_t target, addr_t hook_function,
                         std::size_t code_length) {
  nop(code_length, false);  // placeholder for original instruction(s)
  x86::save_regs(this);
  push(esp);
  mov(eax, hook_function);
  call(eax);
  add(esp, 0x4);
  x86::restore_regs(this);
  push(target + code_length);
  ret();
}

// TODO: fail if code is too short
struct JumpTo : Xbyak::CodeGenerator {
  JumpTo(const u8* target, std::size_t code_length) {
    push(reinterpret_cast<std::uintptr_t>(target));
    ret();
    const std::size_t pad_length = code_length - getSize();
    if (pad_length > 0) {
      nop(pad_length, false);
    }
    dprintf("Trampoline size={}", getSize());
  }
};

Hook::Hook(HookEntry h, hook_fn fn)
    : trampoline_(h.address, reinterpret_cast<addr_t>(fn), h.size) {
  // Create the main hook prologue
  // TODO: use correct signature
  auto* p = trampoline_.getCode<u8*>();

  // Copy original instructions to prologue
  patch_code(p, reinterpret_cast<u8*>(h.address), h.size);

  // Patch target code with jump to hook prologue
  JumpTo D(p, h.size);
  patch_code(reinterpret_cast<u8*>(h.address), D.getCode<u8*>(), D.getSize());
}
