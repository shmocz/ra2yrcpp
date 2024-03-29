#pragma once

#include "types.h"
#ifdef _WIN32
#include "win32/windows_utils.hpp"
#endif

#include <cstddef>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace process {

using thread_id_t = u32;

namespace {
using namespace std::chrono_literals;
}

/// Return current thread id
int get_current_tid();
void* get_current_process_handle();

class Thread {
 public:
  // Open handle to given thread id
  explicit Thread(int thread_id = -1);
  ~Thread();
  void suspend();
  void resume();
  void set_gpr(const x86Reg reg, const int value);
  int* get_pgpr(const x86Reg reg);
  void* handle();
  int id();

 private:
  int id_;
  void* handle_;

 public:
#ifdef _WIN32
  std::unique_ptr<windows_utils::ThreadContext> sysdata_;
#else
  std::unique_ptr<int> sysdata_;
#endif
};

void save_context(Thread* T);
std::string proc_basename(const std::string name);
unsigned long get_pid(void* handle);
/// Get process id by name. If not found, return 0.
unsigned long get_pid(const std::string name);

class Process {
 public:
  // Construct from existing process handle
  explicit Process(void* handle);
  // Open process handle to specified pid
  explicit Process(const u32 pid, const u32 perm = 0u);
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  ~Process();
  unsigned long get_pid() const;
  void* handle() const;
  // Write size bytes from src to dest
  void write_memory(void* dest, const void* src, const std::size_t size,
                    const bool local = false);
  void read_memory(void* dest, const void* src, const std::size_t size);
  // Allocate memory to process
  void* allocate_memory(const std::size_t size, unsigned long alloc_type,
                        unsigned long alloc_protect);
  // Allocate memory to process
  void* allocate_code(const std::size_t size);
  void for_each_thread(std::function<void(Thread*, void*)> callback,
                       void* cb_ctx = nullptr) const;
  // Suspend all threads. If main_tid > -1, suspend if thread's id != main_tid
  void suspend_threads(const thread_id_t tmain_tid = -1,
                       const duration_t delay = 1.0s) const;
  // Suspend all threads, except threads in no_suspend
  void suspend_threads(const std::vector<thread_id_t> no_suspend = {},
                       const duration_t delay = 1.0s) const;
  void resume_threads(const thread_id_t main_tid = -1) const;
  void resume_threads(const std::vector<thread_id_t> no_resume = {}) const;
  std::vector<std::string> list_loaded_modules() const;

 private:
  void* handle_;
};

Process get_current_process();
std::string getcwd();

}  // namespace process
