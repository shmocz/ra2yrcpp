#include "hook.hpp"

#include "errors.hpp"
#include "logging.hpp"
#include "utility/serialize.hpp"
#include "utility/time.hpp"
#include "x86.hpp"

#include <cstdint>

#include <algorithm>
#include <chrono>

using namespace hook;
using namespace std::chrono_literals;

DetourMain::DetourMain(const addr_t target, const addr_t hook,
                       const std::size_t code_length, const addr_t call_hook,
                       unsigned int* count_enter, unsigned int* count_exit) {
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

DetourMain::DetourMain(Hook* h)
    : DetourMain(h->detour().src_address, reinterpret_cast<addr_t>(h),
                 h->detour().code_length, reinterpret_cast<addr_t>(&h->call),
                 h->count_enter(), h->count_exit()) {}

// TODO: fail if code is too short
struct DetourTrampoline : Xbyak::CodeGenerator {
  DetourTrampoline(const u8* target, const std::size_t code_length) {
    push(reinterpret_cast<std::uintptr_t>(target));
    ret();
    const std::size_t pad_length = code_length - getSize();
    if (pad_length > 0) {
      nop(pad_length, false);
    }
    dprintf("Trampoline size={}", getSize());
  }
};

unsigned int num_threads_at_tgt(const process::Process& P, const u8* target,
                                const std::size_t length) {
  auto main_tid = process::get_current_tid();
  std::vector<unsigned int> ips;
  P.for_each_thread([&ips, &main_tid](process::Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != main_tid) {
      ips.push_back(*T->get_pgpr(process::x86Reg::eip));
    }
  });
  unsigned int res = 0;
  const auto t = reinterpret_cast<std::uintptr_t>(target);
  for (auto eip : ips) {
    dprintf("eip,beg,end={},{},{}", eip, t,
            t + static_cast<unsigned int>(length));
    if (eip >= t && (eip < t + length)) {
      ++res;
    }
  }
  return res;
}

Hook::Hook(addr_t src_address, const std::size_t code_length,
           const std::string name, const std::vector<thread_id_t> no_suspend,
           const bool manual)
    : d_{src_address, 0u, code_length},
      name_(name),
      dm_(this),
      no_suspend_(no_suspend),
      count_enter_(0u),
      count_exit_(0u),
      manual_(manual) {
  // Create detour
  auto p = dm_.getCode<u8*>();

  // Copy original instruction to detour
  patch_code(p, reinterpret_cast<u8*>(src_address), code_length);

  // Patch target region
  DetourTrampoline D(p, code_length);
  auto f = D.getCode<u8*>();
  if (manual_) {
    patch_code(reinterpret_cast<u8*>(src_address), f, D.getSize());
  } else {
    patch_code_safe(reinterpret_cast<u8*>(src_address), f, D.getSize());
  }
}

void threads_resume_wait_pause(const process::Process& P,
                               duration_t m = 0.01s) {
  auto main_tid = process::get_current_tid();
  P.resume_threads(main_tid);
  util::sleep_ms(m);
  P.suspend_threads(main_tid);
}

Hook::~Hook() {
  // Remove all callbacks
  lock();
  callbacks().clear();
  unlock();

  // Patch back original code
  auto p = dm_.getCode<u8*>();
  patch_code_safe(reinterpret_cast<u8*>(detour().src_address), p,
                  detour().code_length);
  auto P = process::get_current_process();
  // Wait until all threads have exited the hook
  const auto main_tid = process::get_current_tid();
  P.suspend_threads(main_tid);
  while (num_threads_at_tgt(P, p, detour().code_length) > 0 ||
         (*count_enter() != *count_exit())) {
    threads_resume_wait_pause(P);
  }
  P.resume_threads(main_tid);
}

void Hook::add_callback(HookCallback c) {
  lock();
  callbacks_.push_back(c);
  unlock();
}

void Hook::add_callback(std::function<void(Hook*, void*, X86Regs*)> func,
                        void* user_data, const std::string name,
                        const unsigned max_calls) {
  add_callback(HookCallback{func, user_data, 0u, max_calls, name});
}

void Hook::call(Hook* H, X86Regs state) {
  H->lock();
  unsigned off = 0u;
  auto& C = H->callbacks();
  for (auto i = 0u; i < C.size(); i++) {
    auto ix = i - off;
    auto& c = C.at(ix);
    c.func(H, c.user_data, &state);
    c.calls += 1;
    // remove callbacks whose max_calls count exceeded
    if (c.max_calls > 0u && c.calls >= c.max_calls) {
      C.erase(C.begin() + ix);
      off += 1u;
    }
  }
  H->unlock();
}

std::vector<Hook::HookCallback>& Hook::callbacks() { return callbacks_; }

void Hook::lock() { mu_.lock(); }

void Hook::unlock() { mu_.unlock(); }

Detour& Hook::detour() { return d_; }

const std::string& Hook::name() const { return name_; }

void Hook::patch_code(u8* target_address, const u8* code,
                      const std::size_t code_length) {
  dprintf("address={}, bytes={}", reinterpret_cast<void*>(target_address),
          code_length);
  auto P = process::get_current_process();
  P.write_memory(target_address, code, code_length);
}

void Hook::patch_code_safe(u8* target_address, const u8* code,
                           const std::size_t code_length) {
  auto P = process::get_current_process();
  auto main_tid = process::get_current_tid();

  dprintf("suspending, tgt={}, code={}, len={}, main tid={}",
          reinterpret_cast<void*>(target_address),
          reinterpret_cast<const void*>(code), code_length, main_tid);
  auto ns = std::vector<thread_id_t>(no_suspend_);
  ns.push_back(main_tid);
  P.suspend_threads(ns);
  dprintf("suspend done");
  // FIXME: broken! completely ignores no_suspend_
  // Wait until no thread is at target region
  while (num_threads_at_tgt(P, target_address, code_length) > 0) {
    dprintf("waiting until thread exits target region..");
    threads_resume_wait_pause(P);
  }
  patch_code(target_address, code, code_length);
  P.resume_threads(ns);
}

unsigned int* Hook::count_enter() { return &count_enter_; }

unsigned int* Hook::count_exit() { return &count_exit_; }

void Hook::remove_callback(const std::string name) {
  auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
                         [&name](const auto& j) { return j.name == name; });
  if (it == callbacks_.end()) {
    throw yrclient::general_error("remove_callback");
  }
  callbacks_.erase(it);
}

std::tuple<u32, std::size_t> hook::get_hook_entry(const u32 target) {
  // determine detour size by searching for byte pattern 0x68 <addr> 0xc3
  u32 p_target = serialize::read_obj<u32>(target + 1);
  std::size_t code_size = 0U;
  auto* p = reinterpret_cast<u8*>(p_target);
  constexpr auto pushret_size = 5U;
  for (auto i = 0U; i < DETOUR_MAX_SIZE; i++) {
    if (p[i] == OP_PUSH && p[i + pushret_size] == OP_RET) {  // NOLINT
      code_size = i;
      break;
    }
  }
  return std::make_tuple(p_target, code_size);
}

Callback::Callback() : cpu_state(nullptr) {}

void Callback::call(hook::Hook*, void*, X86Regs* state) {
  cpu_state = state;
  do_call();
  cpu_state = nullptr;
}
