#pragma once

#include "types.h"
#include <mutex>
#include <vector>
#include <xbyak/xbyak.h>

namespace hook {

typedef void(__cdecl* p_cb_hook)(void* hook_object, void *user_data, X86Regs state);

struct UserCallback {
  p_cb_hook func;
  void* args;
};

/// Represents control flow redirection at particular location.
struct Detour {
  addr_t src_address;
  addr_t detour_address;
  size_t code_length;
};

using CodeBuf = u8[8192];

///
/// Install a hook into memory location. This is implementented as a
/// detour that executes user supplied callbacks. When the object is destroyed,
/// the hook is uninstalled with all it's allocated code regions freed.
///
/// On WIN32, detour is installed by suspending all threads, allocating
/// executable memory region, writing the detour "trampoline" to it and patching
/// the code region at target address. If a thread's instruction pointer resides
/// in the target region, execution is redirected to matching instruction
/// within the detour. Uninstallation is done by first removing all callbacks,
/// suspending all threads, restoring original code and freeing the detour
/// memory block. First, it's necessary to wait until none of the threads are
/// in jump region. Once this is satisfied, original code can be patched back,
/// ensuring no thread can longer enter the detour region. If no thread is
/// executing the hook, cleanup can be done directly. Otherwise an exit handler
/// is enabled for the hook so that the last thread exiting from hook performs
/// the cleanup.
///
/// Overall, the code regions are:
/// - jump region: instructions that jump to detour trampoline
/// - detour trampoline: executes the original instruction, pushes return value
/// to stack and jumps to common detour handler
/// - common detour region and call to hook handler
class Hook {
 public:
  struct DetourTrampoline : Xbyak::CodeGenerator {
    DetourTrampoline(const u8* target, const size_t code_length) {
      push(reinterpret_cast<u32>(target));
      ret();
      const size_t pad_length = code_length - getSize();
      if (pad_length > 0) {
        nop(pad_length);
      }
    }
  };

  static std::vector<Xbyak::Reg32> get_regs(Xbyak::CodeGenerator& c) {
    return {c.eax, c.ebx, c.ecx, c.edx, c.esi, c.edi, c.ebp, c.esp};
  }

  static void restore_regs(Xbyak::CodeGenerator& c) {
    for (auto r : get_regs(c)) {
      c.pop(r);
    }
  }

  static void save_regs(Xbyak::CodeGenerator& c) {
    auto regs = get_regs(c);
    for (auto r = regs.rbegin(); r != regs.rend(); r++) {
      c.push(*r);
    }
  }

  struct DetourMain : Xbyak::CodeGenerator {
    DetourMain(const addr_t target, const addr_t hook, const size_t code_length,
               const addr_t call_hook) {
      nop(code_length);  // placeholder for original instruction(s)
      save_regs(*this);
      push(hook);
      mov(eax, call_hook);
      call(eax);
      add(esp, 0x4);
      restore_regs(*this);
      push(target + code_length);
      ret();
    }
    DetourMain(Hook* h)
        : DetourMain(h->detour().src_address, reinterpret_cast<addr_t>(h),
                     h->detour().code_length,
                     reinterpret_cast<addr_t>(&h->call)) {}
  };

  ///
  /// @param src_address Address that will be hooked
  /// @param code_length Number of bytes to copy to detour's location
  ///
  Hook(const addr_t src_address, const size_t code_length);
  ~Hook();
  ///
  void add_callback(UserCallback c);
  /// Invoke all registered hook functions. This function is thread safe.
  static void __cdecl call(Hook* H, X86Regs state);
  std::vector<UserCallback >& callbacks();

  void lock();
  void unlock();
  Detour& detour();
  u8* codebuf();
  constexpr size_t codebuf_length() { return sizeof(codebuf_); }
  void install_jump(const addr_t target, const u8* detour,
                    const size_t code_length);

 private:
  Detour d_;
  std::vector<UserCallback> callbacks_;
  static std::mutex mu_;
  CodeBuf codebuf_;
  DetourMain dm_;
};

}  // namespace hook