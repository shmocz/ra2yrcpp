#include "dll_inject.hpp"
#include <handleapi.h>
#include <tlhelp32.h>
#include <winnt.h>
#include <xbyak/xbyak.h>

using namespace dll_inject;
using process::x86Reg;

void dll_inject::inject_code(process::Process* P, int thread_id,
                             vecu8 shellcode) {
  process::Thread T(thread_id);
  // Save EIP
  auto eip = *T.get_pgpr(x86Reg::eip);
  auto esp = *T.get_pgpr(x86Reg::esp);
  esp -= sizeof(esp);
  P->write_memory(reinterpret_cast<void*>(esp), &eip, sizeof(eip));
  // Allocate memory for shellcode
  auto sc_addr =
      P->allocate_memory(shellcode.size(), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  // Write shellcode
  P->write_memory(sc_addr, shellcode.data(), shellcode.size());
  // Set ESP
  T.set_gpr(x86Reg::esp, esp);
  // Set EIP to shellcode
  T.set_gpr(x86Reg::eip, reinterpret_cast<int>(sc_addr));
}

void dll_inject::suspend_inject_resume(
    handle_t ex_handle, vecu8 shellcode,
    const std::chrono::milliseconds delay_post_suspend,
    const std::chrono::milliseconds delay_post_inject,
    const std::chrono::milliseconds delay_pre_resume) {
  process::Process P(ex_handle);
  P.suspend_threads(-1);
  int tid = 0;
  P.for_each_thread([&tid](process::Thread* T, void* ctx) {
    (void)ctx;
    if (tid == 0) {
      tid = T->id();
    }
  });
  util::sleep_ms(delay_post_suspend);
  inject_code(&P, tid, shellcode);
  util::sleep_ms(delay_post_inject);
  // Resume only the injected thread
  P.for_each_thread([&tid](auto* T, auto* ctx) {
    (void)ctx;
    if (T->id() == tid) {
      T->resume();
    }
  });
  util::sleep_ms(delay_pre_resume);
  // Wait a bit, then resume others
  P.resume_threads(tid);
}
