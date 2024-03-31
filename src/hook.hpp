#pragma once

#include "process.hpp"
#include "types.h"

#include <xbyak/xbyak.h>

#include <cstddef>
#undef ERROR
#undef OK
#include <functional>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace hook {

constexpr unsigned DETOUR_MAX_SIZE = 128U;
constexpr u8 OP_PUSH = 0x68;
constexpr u8 OP_RET = 0xc3;

using process::thread_id_t;

/// Represents control flow redirection at particular location.
struct Detour {
  addr_t src_address;
  addr_t detour_address;
  std::size_t code_length;
};

class Hook;

struct DetourMain : Xbyak::CodeGenerator {
  DetourMain(addr_t target, addr_t hook, std::size_t code_length,
             addr_t call_hook, unsigned int* count_enter,
             unsigned int* count_exit);

  explicit DetourMain(Hook* h);
};

struct HookCallback {
  std::function<void(Hook*, void*, X86Regs*)> func;
  void* user_data;

  // How many times the callback has been invoked.
  unsigned calls{0u};
  std::string name{""};
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

  ///
  /// @param src_address Address that will be hooked
  /// @param code_length Number of bytes to copy to detour's location
  /// @param name (Optional) Name of the hook
  /// @param no_suspend (Optional) List of threads to not suspend during
  /// patching (in addition to current thread id)
  /// TODO: move constructor
  ///
  Hook(addr_t src_address, std::size_t code_length, std::string name = "",
       std::vector<thread_id_t> no_suspend = {}, bool manual = false);
  ~Hook();
  void add_callback(HookCallback c);
  void add_callback(std::function<void(Hook*, void*, X86Regs*)> func,
                    void* user_data, std::string name);

  /// Invoke all registered hook functions. This function is thread safe.
  static void __cdecl call(Hook* H, X86Regs state);
  std::vector<HookCallback>& callbacks();

  void lock();
  void unlock();
  Detour& detour();
  const std::string& name() const;
  void patch_code(u8* target_address, const u8* code, std::size_t code_length);

  /// Wait until no thread is in target region, then patch code.
  void patch_code_safe(u8* target_address, const u8* code,
                       std::size_t code_length);
  /// Pointer to counter for enters to Hook::call.
  unsigned int* count_enter();
  /// Pointer to counter for exits from Hook::call.
  unsigned int* count_exit();
  /// Check if hook has a callback identified by name
  void remove_callback(std::string name);

 private:
  Detour d_;
  std::string name_;
  std::vector<HookCallback> callbacks_;
  std::mutex mu_;
  DetourMain dm_;
  std::vector<thread_id_t> no_suspend_;
  unsigned int count_enter_;
  unsigned int count_exit_;
  bool manual_;
};

struct Callback {
  Callback();
  Callback(const Callback&) = delete;
  Callback& operator=(const Callback&) = delete;
  virtual ~Callback() = default;
  /// The function that will be stored in the HookCallback object
  void call(hook::Hook* h, void* data, X86Regs* state);
  /// Subclasses implement their callback logic by overriding this.
  virtual void do_call() = 0;
  /// Callback's name. Duplicate callbacks will not be added into Hook.
  virtual std::string name() = 0;
  /// Target hook name.
  virtual std::string target() = 0;
  X86Regs* cpu_state;
};

}  // namespace hook
