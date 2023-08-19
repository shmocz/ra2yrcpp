#include "win32/windows_utils.hpp"

#include "logging.hpp"
#include "types.h"
#include "utility/scope_guard.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

typedef void* HWND;

#include <direct.h>
#include <handleapi.h>
#include <libloaderapi.h>
#include <malloc.h>
#include <memoryapi.h>
#include <minwindef.h>
#include <processthreadsapi.h>
#include <psapi.h>
#include <synchapi.h>
#include <tlhelp32.h>
#include <winbase.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace windows_utils;

void* windows_utils::load_library(const std::string name) {
  return LoadLibrary(name.c_str());
}

std::uintptr_t windows_utils::get_proc_address(const std::string addr,
                                               void* module) {
  if (module == nullptr) {
    module = GetModuleHandle(TEXT("kernel32.dll"));
  }
  return reinterpret_cast<std::uintptr_t>(
      GetProcAddress(static_cast<HMODULE>(module), addr.c_str()));
}

// TODO(shmocz): could be static
std::string windows_utils::get_process_name(const int pid) {
  TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
  HANDLE hProcess =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  // Get the process name.
  if (NULL != hProcess) {
    GetProcessImageFileName(hProcess, szProcessName,
                            sizeof(szProcessName) / sizeof(TCHAR));
  }

  // Print the process name and identifier.
  CloseHandle(hProcess);
  return szProcessName;
}

void* windows_utils::open_thread(unsigned long access, bool inherit_handle,
                                 unsigned long thread_id) {
  if (access == 0U) {
    access = THREAD_ALL_ACCESS | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME;
  }
  return OpenThread(access, inherit_handle, thread_id);
}

void* windows_utils::allocate_memory(void* handle, const std::size_t size,
                                     unsigned long alloc_type,
                                     unsigned long alloc_protect) {
  return VirtualAllocEx(handle, NULL, size, alloc_type, alloc_protect);
}

void* windows_utils::allocate_code(void* handle, const std::size_t size) {
  return allocate_memory(handle, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

std::vector<unsigned long> windows_utils::enum_processes() {
  std::vector<unsigned long> res(1024, 0u);
  unsigned long bytes = 0;
  static_assert(sizeof(DWORD) == sizeof(bytes));
  EnumProcesses(res.data(), res.size(), &bytes);
  res.resize(bytes / sizeof(bytes));
  return res;
}

std::string windows_utils::getcwd() {
  char buf[1024];
  _getcwd(buf, sizeof(buf));
  return std::string(buf, strchr(buf, '\0'));
}

std::vector<std::string> windows_utils::list_loaded_modules(
    void* const handle) {
  HMODULE hMods[1024];
  HANDLE hProcess = nullptr;
  DWORD cbNeeded;
  std::vector<std::string> res;

  if (EnumProcessModules(handle, hMods, sizeof(hMods), &cbNeeded)) {
    for (unsigned i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
      TCHAR szModName[MAX_PATH];

      // Get the full path to the module's file.
      if (GetModuleFileNameEx(handle, hMods[i], szModName,
                              sizeof(szModName) / sizeof(TCHAR))) {
        res.push_back(std::string(szModName));
      }
    }
  }

  // Release the handle to the process.
  if (hProcess != nullptr) {
    CloseHandle(hProcess);
  }
  return res;
}

unsigned long windows_utils::suspend_thread(void* handle) {
  return SuspendThread(handle);
}

void* windows_utils::get_current_process_handle() {
  return GetCurrentProcess();
}

int windows_utils::get_current_tid() {
  return static_cast<int>(GetCurrentThreadId());
}

unsigned long windows_utils::resume_thread(void* handle) {
  return ResumeThread(handle);
}

void* windows_utils::open_process(unsigned long access, bool inherit,
                                  unsigned long pid) {
  if (access == 0U) {
    access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
             PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
  }
  return OpenProcess(access, inherit, pid);
}

int windows_utils::read_memory(void* handle, void* dest, const void* src,
                               const std::size_t size) {
  return ReadProcessMemory(handle, src, dest, size, nullptr);
}

int windows_utils::close_handle(void* handle) { return CloseHandle(handle); }

static DWORD vprotect(void* address, const std::size_t size,
                      const DWORD protection) {
  DWORD prot_old{};
  if (!VirtualProtect(address, size, protection, &prot_old)) {
    throw std::runtime_error("VirtualProtect");
  }
  return prot_old;
}

int windows_utils::write_memory(void* handle, void* dest, const void* src,
                                const std::size_t size) {
  return WriteProcessMemory(handle, dest, src, size, nullptr);
}

static MEMORY_BASIC_INFORMATION get_mem_info(const void* address) {
  MEMORY_BASIC_INFORMATION m;
  if (VirtualQuery(address, &m, sizeof(m)) == 0u) {
    throw std::runtime_error("VirtualQuery");
  }
  return m;
}

int windows_utils::write_memory_local(void* dest, const void* src,
                                      const std::size_t size) {
  auto m = get_mem_info(dest);
  DWORD prot_old =
      vprotect(m.BaseAddress, m.RegionSize, PAGE_EXECUTE_READWRITE);
  std::memcpy(dest, src, size);
  (void)vprotect(m.BaseAddress, m.RegionSize, prot_old);
  // FIXME(shmocz): useless
  return 0;
}

unsigned long windows_utils::get_pid(void* handle) {
  return GetProcessId(handle);
}

void windows_utils::for_each_thread(
    std::function<void(ThreadEntry*)> callback) {
  void* hSnapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0U);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("CreateToolHelp32Snapshot");
  }
  THREADENTRY32 te;
  te.dwSize = sizeof(te);
  utility::scope_guard guard = [&hSnapshot]() {
    windows_utils::close_handle(hSnapshot);
  };

  if (Thread32First(hSnapshot, &te)) {
    do {
      ThreadEntry t;
      t.thread_id = te.th32ThreadID;
      t.owner_process_id = te.th32OwnerProcessID;
      callback(&t);
    } while (Thread32Next(hSnapshot, &te));
  } else {
    throw std::runtime_error("Thread32First");
  }
}

static constexpr int divup(int a, int b) { return (a + b - 1) / b; }

constexpr int TD_ALIGN = 16;

ThreadContext::ThreadContext()
    : handle(nullptr), acquired(false), size(0U), data(nullptr) {}

ThreadContext::ThreadContext(void* handle)
    : handle(handle),
      acquired(false),
      size(divup(sizeof(CONTEXT), TD_ALIGN) * TD_ALIGN),
      data(_mm_malloc(size, TD_ALIGN)) {
  std::memset(data, 0, size);
}

ThreadContext::~ThreadContext() { _mm_free(data); }

static int acquire_thread_context(ThreadContext* T, const unsigned long flags) {
  auto* ctx = reinterpret_cast<CONTEXT*>(T->data);
  if (!T->acquired) {
    ctx->ContextFlags = flags;
    int res = GetThreadContext(T->handle, ctx);
    T->acquired = true;
    return res;
  }
  return 0;
}

int ThreadContext::save() {
  return SetThreadContext(handle, reinterpret_cast<CONTEXT*>(data));
}

int* ThreadContext::get_pgpr(const x86Reg reg) {
  auto* ctx = reinterpret_cast<CONTEXT*>(data);
  acquire_thread_context(this, CONTEXT_FULL);
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
    default:
      return nullptr;
  }
}

struct ExProcess::ProcessContext {
  STARTUPINFOA si_;
  PROCESS_INFORMATION pi_;
};

ExProcess::ExProcess(const std::string cmdline, const std::string directory)
    : cmdline_(cmdline),
      directory_(directory),
      ctx(std::make_unique<ProcessContext>()) {
  auto& si_ = ctx->si_;
  auto& pi_ = ctx->pi_;
  std::memset(&si_, 0, sizeof(si_));
  si_.cb = sizeof(si_);
  std::memset(&pi_, 0, sizeof(pi_));
  if (!CreateProcess(
          nullptr,  // No module name (use command line)
          const_cast<char*>(cmdline_.c_str()),  // Command line
          nullptr,  // Process handle not inheritable
          nullptr,  // Thread handle not inheritable
          FALSE,    // Set handle inheritance to FALSE
          0,        // No creation flags
          nullptr,  // Use parent's environment block
          (directory_.empty()
               ? nullptr
               : directory_.c_str()),  // Use parent's starting directory
          &si_,                        // Pointer to STARTUPINFO structure
          &pi_)  // Pointer to PROCESS_INFORMATION structure
  ) {
    throw std::runtime_error("Failed to create process");
  }
}

void* ExProcess::handle() { return ctx->pi_.hProcess; }

void ExProcess::join() {
  if (WaitForSingleObject(handle(), 5000) == WAIT_FAILED) {
    throw std::runtime_error("WaitForSingleObject");
  }
}

unsigned long ExProcess::pid() const { return ctx->pi_.dwProcessId; }

ExProcess::~ExProcess() {
  auto& pi_ = ctx->pi_;
  (void)TerminateProcess(pi_.hProcess, 0);
  if (!(WaitForSingleObject(pi_.hProcess, 5000) == WAIT_OBJECT_0)) {
    eprintf("error with WaitForSingleProcess");
  }
  windows_utils::close_handle(pi_.hProcess);
  windows_utils::close_handle(pi_.hThread);
}
