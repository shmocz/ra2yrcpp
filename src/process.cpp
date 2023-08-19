#include "process.hpp"

#include "errors.hpp"
#include "logging.hpp"
#include "utility.h"
#include "utility/time.hpp"

#include <fmt/core.h>

#include <cstddef>

using namespace process;

#ifdef _WIN32
#include "win32/windows_utils.hpp"

void process::save_context(process::Thread* T) {
  if (T->sysdata_->save() == 0) {
    throw yrclient::system_error("SetThreadContext");
  }
}
#endif

#ifdef __linux__
#include <unistd.h>
#endif

Thread::Thread(int thread_id)
    : id_(thread_id), handle_(nullptr), sysdata_(nullptr) {
#ifdef _WIN32
  handle_ = windows_utils::open_thread(0U, false, thread_id);
  if (handle_ == nullptr) {
    throw yrclient::system_error("open_thread");
  }
  sysdata_ = std::make_unique<windows_utils::ThreadContext>(handle_);
#elif __linux__
#else
#error Not implemented
#endif
}

Thread::~Thread() {
#ifdef _WIN32
  windows_utils::close_handle(handle_);
#elif __linux__
#else
#error Not implemented
#endif
}

static unsigned long suspend_thread(void* handle) {
#ifdef _WIN32
  return windows_utils::suspend_thread(handle);
#elif __linux__
  return 1;
#else
#error Not implemented
#endif
}

void* process::get_current_process_handle() {
#ifdef _WIN32
  return windows_utils::get_current_process_handle();
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
}

int process::get_current_tid() {
#ifdef _WIN32
  return windows_utils::get_current_tid();
#elif __linux__
  return -1;
#else
#error Not implemented
#endif
}

void Thread::suspend() {
#ifdef _WIN32
  dprintf("tid,handle={},{}", id(), handle());
  if (suspend_thread(handle()) == (unsigned long)-1) {
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
  (void)windows_utils::resume_thread(handle());
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
  return sysdata_->get_pgpr(reg);
#else
  return nullptr;
#endif
}

void* Thread::handle() { return handle_; }

int Thread::id() { return id_; }

Process::Process(void* handle) : handle_(handle) {}

Process::Process(const u32 pid, const u32 perm) : Process(nullptr) {
#ifdef _WIN32
  u32 p = perm;
  handle_ = windows_utils::open_process(p, false, pid);
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

std::string process::proc_basename(const std::string name) {
  auto pos = name.rfind("\\");
  pos = pos == std::string::npos ? 0u : pos;
  return name.substr(pos + 1);
}

unsigned long process::get_pid(void* handle) {
#ifdef _WIN32
  return windows_utils::get_pid(handle);
#elif __linux__
  return 1;
#else
#error Not implemented
#endif
}

unsigned long process::get_pid(const std::string name) {
#ifdef _WIN32
  auto plist = windows_utils::enum_processes();
  for (auto pid : plist) {
    auto n =
        proc_basename(windows_utils::get_process_name(static_cast<int>(pid)));
    if (n == name) {
      return pid;
    }
  }
  return 0;
#else
  return 0;
#endif
}

unsigned long Process::get_pid() const { return process::get_pid(handle()); }

void* Process::handle() const { return handle_; }

void Process::write_memory(void* dest, const void* src, const std::size_t size,
                           const bool local) {
#ifdef _WIN32
  if (local) {
    windows_utils::write_memory_local(dest, src, size);
  } else {
    if (windows_utils::write_memory(handle_, dest, src, size) == 0) {
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
void Process::read_memory(void* dest, const void* src, const std::size_t size) {
#ifdef _WIN32
  if (windows_utils::read_memory(handle_, dest, src, size) == 0) {
    throw yrclient::system_error(
        fmt::format("ReadProcessMemory src={},count={}", src, size));
  }
#elif __linux__
  return;
#else
#error Not implemented
#endif
}

void* Process::allocate_memory(const std::size_t size, unsigned long alloc_type,
                               unsigned long alloc_protect) {
#ifdef _WIN32
  return windows_utils::allocate_memory(handle_, size, alloc_type,
                                        alloc_protect);
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
}

void* Process::allocate_code(const std::size_t size) {
#ifdef _WIN32
  return windows_utils::allocate_code(handle_, size);
#elif __linux__
  return nullptr;
#else
#error Not implemented
#endif
}

void Process::for_each_thread(std::function<void(Thread*, void*)> callback,
                              void* cb_ctx) const {
#ifdef _WIN32
  (void)cb_ctx;
  const auto pid = get_pid();
  windows_utils::for_each_thread([&](auto* te) {
    if (te->owner_process_id == pid) {
      Thread thread(te->thread_id);
      callback(&thread, cb_ctx);
    }
  });
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
#ifdef _WIN32
  for_each_thread([main_tid](Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != static_cast<int>(main_tid)) {
      T->resume();
    }
  });
#endif
}

void Process::resume_threads(const std::vector<thread_id_t> no_resume) const {
#ifdef _WIN32
  for_each_thread([&no_resume](Thread* T, void* ctx) {
    (void)ctx;
    if (!yrclient::contains(no_resume, T->id())) {
      T->resume();
    }
  });
#endif
}

std::vector<std::string> Process::list_loaded_modules() const {
#ifdef _WIN32
  return windows_utils::list_loaded_modules(handle());
#elif __linux__
  return {};
#else
#error Not implemented
#endif
}

std::string process::getcwd() {
#ifdef _WIN32
  return windows_utils::getcwd();
#elif __linux__
  char buf[1024];
  ::getcwd(buf, sizeof(buf));
  return std::string(buf, strchr(buf, '\0'));
#else
#error Not implemented
#endif
}
