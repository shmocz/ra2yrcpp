#include "hook.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "process.hpp"
#include "utility/time.hpp"
#include <chrono>
#include <string>
#include <thread>

using namespace hook;
using namespace std::chrono_literals;

unsigned int num_threads_at_tgt(const process::Process& P, const u8* target,
                                const size_t length) {
  auto main_tid = process::get_current_tid();
  std::vector<unsigned int> ips;
  P.for_each_thread([&ips, &main_tid](process::Thread* T, void* ctx) {
    if (T->id() != main_tid) {
      ips.push_back(*T->get_pgpr(process::x86Reg::eip));
    }
  });
  unsigned int res = 0;
  const auto t = reinterpret_cast<unsigned int>(target);
  for (auto eip : ips) {
    DPRINTF("eip,beg,end=%x,%x,%x\n", eip, t, t + length);
    if (eip >= t && (eip < t + length)) {
      ++res;
    }
  }
  return res;
}

Hook::Hook(addr_t src_address, const size_t code_length, const std::string name,
           const std::vector<thread_id_t> no_suspend)
    : d_{src_address, 0u, code_length},
      name_(name),
      dm_(this),
      no_suspend_(no_suspend),
      count_enter_(0u),
      count_exit_(0u) {
  auto s = yrclient::join_string(utility::vmap(
      no_suspend_, [](auto a) -> std::string { return yrclient::to_hex(a); }));
  DPRINTF("src_address=%p, length=%d, name=%s, masked_tids=%s\n", src_address,
          code_length, name.c_str(), s.c_str());
  // Create detour
  auto p = dm_.getCode<u8*>();

  // Copy original instruction to detour
  patch_code(p, reinterpret_cast<u8*>(src_address), code_length);

  // Patch target region
  DetourTrampoline D(p, code_length);
  auto f = D.getCode<u8*>();
  patch_code_safe(reinterpret_cast<u8*>(src_address), f, D.getSize());
}

void threads_resume_wait_pause(const process::Process& P,
                               std::chrono::milliseconds m = 10ms) {
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
  DPRINTF("Restoring original code\n");
  patch_code_safe(reinterpret_cast<u8*>(detour().src_address), p,
                  detour().code_length);
  DPRINTF("Restored original code\n");
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

void Hook::call(Hook* H, X86Regs state) {
  H->lock();
  for (auto& c : H->callbacks()) {
    c.func(H, c.user_data, &state);
  }
  H->unlock();
}

std::vector<Hook::HookCallback>& Hook::callbacks() { return callbacks_; }
void Hook::lock() { mu_.lock(); }
void Hook::unlock() { mu_.unlock(); }
Detour& Hook::detour() { return d_; }
const std::string& Hook::name() const { return name_; }

u8* Hook::codebuf() { return codebuf_; }

void Hook::patch_code(u8* target_address, const u8* code,
                      const size_t code_length) {
  DPRINTF("patch at %p, bytes=%d\n", target_address, code_length);
  auto P = process::get_current_process();
  P.write_memory(target_address, code, code_length);
}

void Hook::patch_code_safe(u8* target_address, const u8* code,
                           const size_t code_length) {
  auto P = process::get_current_process();
  auto main_tid = process::get_current_tid();

  DPRINTF("suspending, tgt=%p, code=%p, len=%ld, main tid=%x\n", target_address,
          code, code_length, main_tid);
  auto ns = std::vector<thread_id_t>(no_suspend_);
  ns.push_back(main_tid);
  P.suspend_threads(ns);
  DPRINTF("suspend done\n");
  // Wait until no thread is at target region
  while (num_threads_at_tgt(P, target_address, code_length) > 0) {
    DPRINTF("waiting until thread exits target region..\n");
    threads_resume_wait_pause(P);
  }
  patch_code(target_address, code, code_length);
  P.resume_threads(ns);
}

unsigned int* Hook::count_enter() { return &count_enter_; }
unsigned int* Hook::count_exit() { return &count_exit_; }
