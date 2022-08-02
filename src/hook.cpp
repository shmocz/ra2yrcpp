#include "hook.hpp"

using namespace hook;
using namespace std::chrono_literals;

unsigned int num_threads_at_tgt(const process::Process& P, const u8* target,
                                const size_t length) {
  auto main_tid = process::get_current_tid();
  std::vector<unsigned int> ips;
  P.for_each_thread([&ips, &main_tid](process::Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != main_tid) {
      ips.push_back(*T->get_pgpr(process::x86Reg::eip));
    }
  });
  unsigned int res = 0;
  const auto t = reinterpret_cast<unsigned int>(target);
  for (auto eip : ips) {
    DPRINTF("eip,beg,end=%x,%x,%x\n", eip, t,
            t + static_cast<unsigned int>(length));
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
                      const size_t code_length) {
  DPRINTF("patch at %p, bytes=%lu\n", target_address,
          static_cast<unsigned long>(code_length));
  auto P = process::get_current_process();
  P.write_memory(target_address, code, code_length);
}

void Hook::patch_code_safe(u8* target_address, const u8* code,
                           const size_t code_length) {
  auto P = process::get_current_process();
  auto main_tid = process::get_current_tid();

  DPRINTF("suspending, tgt=%p, code=%p, len=%lu, main tid=%x\n", target_address,
          code, static_cast<unsigned long>(code_length), main_tid);
  auto ns = std::vector<thread_id_t>(no_suspend_);
  ns.push_back(main_tid);
  P.suspend_threads(ns);
  DPRINTF("suspend done\n");
  // FIXME: broken! completely ignores no_suspend_
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

template <typename T>
static auto get_callback_(T* h, const std::string name) {
  return std::find_if(h->begin(), h->end(),
                      [&name](const auto& j) { return j.name == name; });
}

void Hook::remove_callback(const std::string name) {
  auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
                         [&name](const auto& j) { return j.name == name; });
  if (it == callbacks_.end()) {
    throw yrclient::general_error("remove_callback");
  }
  callbacks_.erase(it);
}
