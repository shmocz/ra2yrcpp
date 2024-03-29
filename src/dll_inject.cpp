#include "dll_inject.hpp"

#include "process.hpp"
#include "types.h"
#include "utility/time.hpp"

#include <stdexcept>

using namespace dll_inject;

void dll_inject::inject_code(process::Process* P, int thread_id,
                             vecu8 shellcode) {
  process::Thread T(thread_id);
  // Save EIP
  auto eip = *T.get_pgpr(x86Reg::eip);
  auto esp = *T.get_pgpr(x86Reg::esp);
  esp -= sizeof(esp);
  P->write_memory(reinterpret_cast<void*>(esp), &eip, sizeof(eip));
  // Allocate memory for shellcode
  auto sc_addr = P->allocate_code(shellcode.size());
  // Write shellcode
  P->write_memory(sc_addr, shellcode.data(), shellcode.size());
  // Set ESP
  T.set_gpr(x86Reg::esp, esp);
  // Set EIP to shellcode
  T.set_gpr(x86Reg::eip, reinterpret_cast<int>(sc_addr));
}

void dll_inject::suspend_inject_resume(handle_t ex_handle, vecu8 shellcode,
                                       const DLLInjectOptions o) {
  process::Process P(ex_handle);
  P.suspend_threads(-1);
  int tid = -1;
  P.for_each_thread([&tid](process::Thread* T, void*) {
    if (tid == -1) {
      tid = T->id();
    }
  });
  if (tid == -1) {
    throw std::runtime_error("tid -1");
  }
  util::sleep_ms(o.delay_post_suspend);
  inject_code(&P, tid, shellcode);
  util::sleep_ms(o.delay_post_inject);
  // Resume only the injected thread
  P.for_each_thread([&tid](auto* T, auto* ctx) {
    (void)ctx;
    if (T->id() == tid) {
      T->resume();
    }
  });
  util::sleep_ms(o.delay_pre_resume);
  // Wait a bit, then resume others
  P.resume_threads(tid);
}
