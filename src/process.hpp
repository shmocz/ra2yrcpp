#pragma once

#include "errors.hpp"
#include "types.h"

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace process {

using thread_id_t = u32;
using std::size_t;

namespace {
using namespace std::chrono_literals;
}

enum class x86Reg : int {
  eax = 0,
  ebx = 1,
  ecx = 2,
  edx = 3,
  esi = 4,
  edi = 5,
  ebp = 6,
  esp = 7,
  eip = 8
};

/// Suspend thread indicated by given handle. Returns 0 on success, nonzero on
/// error.
unsigned long suspend_thread(void* handle);
void* open_thread(unsigned long access, bool inherit_handle,
                  unsigned long thread_id);
/// Return current thread id
int get_current_tid();
void* get_current_process_handle();
std::vector<u32> get_process_list();
std::string get_process_name(const u32 pid);

/// Platform-specific thread data (like CONTEXT on WIN32)
struct ThreadData {
  ThreadData();
  ~ThreadData();
  void* data;
  size_t size;
};

#ifndef _WIN32
constexpr unsigned int TD_SIZE = 1;
constexpr unsigned int TD_ALIGN = 16;
#endif

#pragma pack(push, 16)

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
  ThreadData& thread_data();
  // CONTEXT& acquire_context(const DWORD flags = CONTEXT_FULL);
  // void save_context();

 private:
  int id_;
  void* handle_;
  // CONTEXT ctx;
  ThreadData sysdata_;
  // targetThread = ;
};

#pragma pack(pop)

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
  void write_memory(void* dest, const void* src, const size_t size,
                    const bool local = false);
  void read_memory(void* dest, const void* src, const size_t size);
  // Allocate memory to process
  void* allocate_memory(const size_t size, unsigned long alloc_type,
                        unsigned long alloc_protect);
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
