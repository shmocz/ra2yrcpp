#pragma once

#include "errors.hpp"
#include "logging.hpp"
#include "process.hpp"
#include "types.h"
#include "utility/time.hpp"
#include "x86.hpp"

#include <xbyak/xbyak.h>
#undef ERROR
#undef OK
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace hook {

using process::thread_id_t;

/// TODO: verify that a region is writable before attempting to write to it

/// Represents control flow redirection at particular location.
struct Detour {
  addr_t src_address;
  addr_t detour_address;
  size_t code_length;
};

///
/// Install a hook into memory location. This is implementented as a
/// detour that executes user supplied callbacks. When the object is destroyed,
/// the hook is uninstalled with all it's allocated code regions freed.
///
/// On WIN32, detour is installed by suspending all threads, allocating
/// executable memory region, writing the detour "trampoline" to it and patching
/// the code region at target address. If thread's instruction pointer is in
/// target region, suspend/resume is called repeatedly until it has exited
/// the region. Uninstallation is done by first removing all callbacks,
/// suspending all threads, restoring original code and freeing the detour
/// memory block. Detour trampoline is removed in same way as it was originally
/// installed. Once all threads have exited from the detour region, code region
/// is freed and Hook object destroyed.
///
class Hook {
 public:
  typedef void (*hook_cb_t)(Hook* h, void* user_data, X86Regs* state);

  struct HookCallback {
    std::function<void(Hook*, void*, X86Regs*)> func;
    void* user_data;

    // How many times the callback has been invoked.
    unsigned calls{0u};
    // How many times to invoke the callback before removal. 0 = never.
    unsigned max_calls{0u};
    std::string name{""};
  };

  // TODO: fail if code is too short
  struct DetourTrampoline : Xbyak::CodeGenerator {
    DetourTrampoline(const u8* target, const size_t code_length) {
      push(reinterpret_cast<u32>(target));
      ret();
      const size_t pad_length = code_length - getSize();
      if (pad_length > 0) {
        nop(pad_length, false);
      }
      dprintf("Trampoline size={}", getSize());
    }
  };

  struct DetourMain : Xbyak::CodeGenerator {
    DetourMain(const addr_t target, const addr_t hook, const size_t code_length,
               const addr_t call_hook, unsigned int* count_enter,
               unsigned int* count_exit) {
      nop(code_length, false);  // placeholder for original instruction(s)
      x86::save_regs(this);
      push(hook);
      mov(eax, call_hook);
      lock();
      inc(dword[count_enter]);
      call(eax);
      lock();
      inc(dword[count_exit]);
      add(esp, 0x4);
      x86::restore_regs(this);
      push(target + code_length);
      ret();
    }

    explicit DetourMain(Hook* h)
        : DetourMain(h->detour().src_address, reinterpret_cast<addr_t>(h),
                     h->detour().code_length,
                     reinterpret_cast<addr_t>(&h->call), h->count_enter(),
                     h->count_exit()) {}
  };

  ///
  /// @param src_address Address that will be hooked
  /// @param code_length Number of bytes to copy to detour's location
  /// @param name (Optional) Name of the hook
  /// @param no_suspend (Optional) List of threads to not suspend during
  /// patching (in addition to current thread id)
  /// TODO: move constructor
  ///
  Hook(addr_t src_address, const size_t code_length,
       const std::string name = "",
       const std::vector<thread_id_t> no_suspend = {});
  ~Hook();
  void add_callback(HookCallback c);
  ///
  void add_callback(std::function<void(Hook*, void*, X86Regs*)> func,
                    void* user_data, const std::string name,
                    const unsigned max_calls = 0u);

  /// Invoke all registered hook functions. This function is thread safe.
  static void __cdecl call(Hook* H, X86Regs state);
  std::vector<HookCallback>& callbacks();

  void lock();
  void unlock();
  Detour& detour();
  const std::string& name() const;
  void patch_code(u8* target_address, const u8* code, const size_t code_length);

  /// Wait until no thread is in target region, then patch code.
  void patch_code_safe(u8* target_address, const u8* code,
                       const size_t code_length);
  /// Pointer to counter for enters to Hook::call.
  unsigned int* count_enter();
  /// Pointer to counter for exits from Hook::call.
  unsigned int* count_exit();
  /// Check if hook has a callback identified by name
  void remove_callback(const std::string name);

 private:
  Detour d_;
  const std::string name_;
  std::vector<HookCallback> callbacks_;
  std::mutex mu_;
  DetourMain dm_;
  std::vector<thread_id_t> no_suspend_;
  unsigned int count_enter_;
  unsigned int count_exit_;
};

}  // namespace hook
