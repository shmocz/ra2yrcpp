#include "process.hpp"

using namespace process;
using yrclient::not_implemented;

#ifdef _WIN32
#include <io.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
constexpr int TD_ALIGN = 16;

#ifdef _MSC_VER
#include <direct.h>  // _getcwd
#endif

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

void process::save_context(process::Thread* T) {
  auto& D = T->thread_data();
  auto& ctx = (static_cast<TData*>(D.data))->ctx;
  if (SetThreadContext(T->handle(), &ctx) == 0) {
    throw yrclient::system_error("SetThreadContext");
  }
}
#endif

#ifdef __linux__
#include <unistd.h>
#endif

std::vector<u32> process::get_process_list() {
#ifdef _WIN32
  std::vector<u32> res(1024, 0u);
  u32 bytes = 0;
  static_assert(sizeof(DWORD) == sizeof(u32));
  EnumProcesses(reinterpret_cast<DWORD*>(res.data()), (DWORD)res.size(),
                reinterpret_cast<DWORD*>(&bytes));
  res.resize(bytes / sizeof(u32));
  // cerr << "Actual list size " << res.size() << endl;
  return res;
#elif __linux__
  return {};
#else
#error Not implemented
#endif
}

std::string process::get_process_name(const u32 pid) {
#ifdef _WIN32
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
#elif __linux__
  (void)pid;
  return "";
#else
#error Not implemented
#endif
}

ThreadData::ThreadData()
    : data(nullptr), size(yrclient::divup(TD_SIZE, TD_ALIGN) * TD_ALIGN) {
#ifdef _WIN32
  data = _mm_malloc(size, TD_ALIGN);
  memset(data, 0, size);
#elif __linux__
#else
#error Not implemented
#endif
}

ThreadData::~ThreadData() {
#ifdef _WIN32
  _mm_free(data);
#elif __linux__
#else
#error Not implemented
#endif
}

Thread::Thread(int thread_id) : id_(thread_id), handle_(nullptr) {
#ifdef _WIN32
  handle_ = process::open_thread(
      THREAD_ALL_ACCESS | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE,
      thread_id);
#elif __linux__
#else
#error Not implemented
#endif
}

Thread::~Thread() {
#ifdef _WIN32
  CloseHandle(handle_);
#elif __linux__
#else
#error Not implemented
#endif
}

unsigned long process::suspend_thread(void* handle) {
#ifdef _WIN32
  return SuspendThread(handle);
#elif __linux__
  return 1;
#else
#error Not implemented
#endif
}

void* process::open_thread(unsigned long access, bool inherit_handle,
                           unsigned long thread_id) {
#ifdef _WIN32
  void* h = OpenThread(access, inherit_handle, thread_id);
  if (h == nullptr) {
    throw yrclient::system_error("open_thread");
  }
  return h;
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
}

void* process::get_current_process_handle() {
#ifdef _WIN32
  return GetCurrentProcess();
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
}

int process::get_current_tid() {
#ifdef _WIN32
  return GetCurrentThreadId();
#elif __linux__
  return -1;
#else
#error Not implemented
#endif
}

void Thread::suspend() {
#ifdef _WIN32
  dprintf("tid,handle={},{}", id(), handle());
  if (suspend_thread(handle()) == (DWORD)-1) {
    throw yrclient::system_error("suspend_thread");
  }
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void Thread::resume() {
#ifdef _WIN32
  dprintf("tid,handle={},{}", id(), handle());
  ResumeThread(handle());
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void Thread::set_gpr(const x86Reg reg, const int value) {
#ifdef _WIN32
  auto* c = get_pgpr(reg);
  *c = value;
  save_context(this);
#elif __linux__
  return;
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
    default:
      return nullptr;
  }
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
}

void* Thread::handle() { return handle_; }

int Thread::id() { return id_; }

ThreadData& Thread::thread_data() { return sysdata_; }

Process::Process(void* handle) : handle_(handle) {}

Process::Process(const u32 pid, const u32 perm) : Process(nullptr) {
#ifdef _WIN32
  u32 p = perm;
  if (p == 0u) {
    p = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
  }
  handle_ = OpenProcess(p, FALSE, pid);
  if (handle_ == nullptr) {
    throw yrclient::system_error("OpenProcess");
  }
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

Process::~Process() {}

Process process::get_current_process() {
  return Process(process::get_current_process_handle());
}

#ifdef _WIN32
static MEMORY_BASIC_INFORMATION get_mem_info(const void* address) {
  MEMORY_BASIC_INFORMATION m;
  if (VirtualQuery(address, &m, sizeof(m)) == 0u) {
    throw yrclient::system_error("VirtualQuery");
  }
  return m;
}
#elif __linux__
#else
#error Not implemented
#endif

#ifdef _WIN32
static DWORD vprotect(void* address, const size_t size,
                      const DWORD protection) {
  DWORD prot_old;
  if (!VirtualProtect(address, size, protection, &prot_old)) {
    throw yrclient::system_error("VirtualProtect");
  }
  return prot_old;
}
#elif __linux__
#else
#error Not implemented
#endif

std::string process::proc_basename(const std::string name) {
  auto pos = name.rfind("\\");
  pos = pos == std::string::npos ? 0u : pos;
  return name.substr(pos + 1);
}

unsigned long process::get_pid(void* handle) {
#ifdef _WIN32
  return GetProcessId(handle);
#elif __linux__
  return 1;
#else
#error Not implemented
#endif
}

unsigned long process::get_pid(const std::string name) {
  auto plist = process::get_process_list();
  for (auto pid : plist) {
    auto n = proc_basename(process::get_process_name(pid));
    if (n == name) {
      return pid;
    }
  }
  return 0;
}

unsigned long Process::get_pid() const { return process::get_pid(handle()); }

void* Process::handle() const { return handle_; }

void Process::write_memory(void* dest, const void* src, const size_t size,
                           const bool local) {
#ifdef _WIN32
  if (local) {
    auto m = get_mem_info(dest);
    DWORD prot_old =
        vprotect(m.BaseAddress, m.RegionSize, PAGE_EXECUTE_READWRITE);
    std::memcpy(dest, src, size);
    (void)vprotect(m.BaseAddress, m.RegionSize, prot_old);
  } else {
    if (WriteProcessMemory(handle_, dest, src, size, nullptr) == 0) {
      throw yrclient::system_error("WriteProcesMemory");
    }
  }
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

// cppcheck-suppress unusedFunction
void Process::read_memory(void* dest, const void* src, const size_t size) {
#ifdef _WIN32
  if (ReadProcessMemory(handle_, src, dest, size, nullptr) == 0) {
    throw yrclient::system_error("ReadProcessMemory src=" +
                                 yrclient::to_hex(reinterpret_cast<u32>(src)) +
                                 ",count=" + std::to_string(size));
  }
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void* Process::allocate_memory(const size_t size, unsigned long alloc_type,
                               unsigned long alloc_protect) {
#ifdef _WIN32
  return VirtualAllocEx(handle_, NULL, size, alloc_type, alloc_protect);
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
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
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void Process::suspend_threads(const thread_id_t main_tid,
                              const duration_t delay) const {
#ifdef _WIN32
  suspend_threads(std::vector<thread_id_t>{main_tid}, delay);
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void Process::suspend_threads(const std::vector<thread_id_t> no_suspend,
                              const duration_t delay) const {
#ifdef _WIN32
  util::sleep_ms(delay);
  for_each_thread([&no_suspend](Thread* T, void* ctx) {
    (void)ctx;
    if (!yrclient::contains(no_suspend, T->id())) {
      T->suspend();
    } else {
      dprintf("not suspending masked thread {}", T->id());
    }
  });
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void Process::resume_threads(const thread_id_t main_tid) const {
  for_each_thread([main_tid](Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != static_cast<int>(main_tid)) {
      T->resume();
    }
  });
}

void Process::resume_threads(const std::vector<thread_id_t> no_resume) const {
  for_each_thread([&no_resume](Thread* T, void* ctx) {
    (void)ctx;
    if (!yrclient::contains(no_resume, T->id())) {
      T->resume();
    }
  });
}

// pretty much copypaste from
// https://docs.microsoft.com/en-us/windows/win32/psapi/enumerating-all-modules-for-a-process
std::vector<std::string> Process::list_loaded_modules() const {
#ifdef _WIN32
  HMODULE hMods[1024];
  HANDLE hProcess;
  DWORD cbNeeded;
  std::vector<std::string> res;

  if (EnumProcessModules(handle(), hMods, sizeof(hMods), &cbNeeded)) {
    for (unsigned i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
      TCHAR szModName[MAX_PATH];

      // Get the full path to the module's file.
      if (GetModuleFileNameEx(handle(), hMods[i], szModName,
                              sizeof(szModName) / sizeof(TCHAR))) {
        res.push_back(std::string(szModName));
      }
    }
  }
  return res;

  // Release the handle to the process.

  CloseHandle(hProcess);
#elif __linux__
  return {};
#else
#error Not implemented
#endif
}

std::string process::getcwd() {
  char buf[1024];
#ifdef _WIN32
#ifndef _MSC_VER
  ::getcwd(buf, sizeof(buf));
#else
  _getcwd(buf, sizeof(buf));
#endif
#elif __linux__
  ::getcwd(buf, sizeof(buf));
#else
#error Not implemented
#endif
  return std::string(buf, strchr(buf, '\0'));
}
