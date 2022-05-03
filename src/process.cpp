#include "process.hpp"
#include "debug_helpers.h"
#include <thread>
#include <chrono>
#ifdef _WIN32
#include "utility.h"
#include "utility/scope_guard.hpp"
#include "utility/time.hpp"
#include "errors.hpp"
#include <cstdlib>
#include <processthreadsapi.h>
#include <tlhelp32.h>
#include <windows.h>
constexpr int TD_ALIGN = 16;

#pragma pack(16)
struct TData {
  bool acquired;
  CONTEXT ctx;
};
constexpr size_t TD_SIZE = sizeof(TData);

CONTEXT* acquire_context(process::Thread* T, const DWORD flags = CONTEXT_FULL) {
  auto& D = T->thread_data();
  auto* TD = static_cast<TData*>(D.data);
  auto& ctx = TD->ctx;
  if (!TD->acquired) {
    ctx.ContextFlags = flags;
    if (GetThreadContext(T->handle(), &ctx) == 0) {
      throw yrclient::system_error("GetThreadContext");
    }
    TD->acquired = true;
  }
  return &ctx;
}

void save_context(process::Thread* T) {
  auto& D = T->thread_data();
  auto& ctx = (static_cast<TData*>(D.data))->ctx;
  if (SetThreadContext(T->handle(), &ctx) == 0) {
    throw yrclient::system_error("SetThreadContext");
  }
}
#endif

using namespace std::chrono_literals;
using namespace process;
using yrclient::not_implemented;

ThreadData::ThreadData()
    : data(nullptr), size(yrclient::divup(TD_SIZE, TD_ALIGN) * TD_ALIGN) {
  data = _mm_malloc(size, TD_ALIGN);
  memset(data, 0, size);
}
ThreadData::~ThreadData() { _mm_free(data); }

Thread::Thread(int thread_id) : id_(thread_id) {
#ifdef _WIN32
  handle_ = process::open_thread(
      THREAD_ALL_ACCESS | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE,
      thread_id);
#else
#error Not implemented
#endif
}
Thread::~Thread() {
#ifdef _WIN32
  CloseHandle(handle_);
#else
#error Not implemented
#endif
}

unsigned long process::suspend_thread(void* handle) {
#ifdef _WIN32
  return SuspendThread(handle);
#else
#error Not implemented
#endif
}

void* process::open_thread(unsigned long access, bool inherit_handle,
                           unsigned long thread_id) {
  void* h = OpenThread(access, inherit_handle, thread_id);
  if (h == nullptr) {
    throw yrclient::system_error("open_thread");
  }
  return h;
}

void* process::get_current_process_handle() {
#ifdef _WIN32
  return GetCurrentProcess();
#else
#error Not implemented
#endif
}

int process::get_current_tid() {
#ifdef _WIN32
  return GetCurrentThreadId();
#else
#error Not implemented
#endif
}

void Thread::suspend() {
  DPRINTF("tid,handle=%x,%p\n", id(), handle());
  if (suspend_thread(handle()) == (DWORD)-1) {
    throw yrclient::system_error("suspend_thread");
  }
}

void Thread::resume() {
#ifdef _WIN32
  DPRINTF("tid,handle=%x,%p\n", id(), handle());
  ResumeThread(handle());
#else
#error Not implemented
#endif
}
void Thread::set_gpr(const x86Reg reg, const int value) {
#ifdef _WIN32
  auto* c = get_pgpr(reg);
  *c = value;
  save_context(this);
#else
#error Not implemented
#endif
}
int* Thread::get_pgpr(const x86Reg reg) {
#ifdef _WIN32
  auto* ctx = acquire_context(this);
  switch (reg) {
#define I(a, b)   \
  case x86Reg::a: \
    return reinterpret_cast<int*>(&ctx->b)
    I(eax, Eax);
    I(ebx, Ebx);
    I(ecx, Ecx);
    I(edx, Edx);
    I(esi, Esi);
    I(edi, Edi);
    I(ebp, Ebp);
    I(esp, Esp);
    I(eip, Eip);
#undef I
#else
#error Not implemented
#endif
  }
  return nullptr;
}

void* Thread::handle() { return handle_; }
int Thread::id() { return id_; }
ThreadData& Thread::thread_data() { return sysdata_; }

Process::Process(void* handle) : handle_(handle) {}
Process::Process(const int pid) {}
Process::~Process() {}
Process process::get_current_process() {
  return Process(process::get_current_process_handle());
}
unsigned long Process::get_pid() const { return GetProcessId(handle()); }
void* Process::handle() const { return handle_; }
void Process::write_memory(void* dest, const void* src, const size_t size) {
  throw yrclient::not_implemented();
}
void Process::for_each_thread(std::function<void(Thread*, void*)> callback,
                              void* cb_ctx) const {
#ifdef _WIN32
  HANDLE hSnapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    throw yrclient::system_error("CreateToolhelp32Snapshot");
  }
  THREADENTRY32 te;
  te.dwSize = sizeof(te);
  utility::scope_guard guard = [&hSnapshot]() { CloseHandle(hSnapshot); };
  const DWORD pid = get_pid();

  if (Thread32First(hSnapshot, &te)) {
    do {
      if (te.th32OwnerProcessID == pid) {
        Thread thread(te.th32ThreadID);
        callback(&thread, cb_ctx);
      }
    } while (Thread32Next(hSnapshot, &te));
  } else {
    throw yrclient::system_error("Thread32First");
  }
#else
#error Not implemented
#endif
}
void Process::suspend_threads(const int main_tid,
                              const std::chrono::milliseconds delay) const {
#ifdef _WIN32
  util::sleep_ms(delay);
  for_each_thread([main_tid](Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != main_tid) {
      T->suspend();
    }
  });
#else
#endif
}
void Process::resume_threads(const int main_tid) const {
#ifdef _WIN32
  for_each_thread([main_tid](Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != main_tid) {
      T->resume();
    }
  });
#else
#endif
}
