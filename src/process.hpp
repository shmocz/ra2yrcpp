#pragma once

#include <functional>

namespace process {

using std::size_t;

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
/// Return current thread id
int get_current_tid();
void* get_current_process_handle();

/// Platform-specific thread data (like CONTEXT on WIN32)
struct ThreadData {
  ThreadData();
  ~ThreadData();
  void* data;
  size_t size;
};

#pragma pack(16)
class Thread {
 public:
  // Open handle to given thread id
  Thread(int thread_id = -1);
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

class Process {
 public:
  // Construct from existing process handle
  Process(void* handle);
  // Open process handle to specified pid
  Process(const int pid);
  // Copy constructor
  Process(Process& o) = default;
  // Copy assignment
  ~Process();
  unsigned long get_pid();
  void* handle();
  // Write size bytes from src to dest
  void write_memory(void* dest, const void* src, const size_t size);
  // Allocate memory to process
  // LPVOID allocate_memory(const size_t size, DWORD alloc_type,
  //                       DWORD alloc_protect);
  void for_each_thread(std::function<void(Thread*, void*)> callback,
                       void* cb_ctx = nullptr);
  // Suspend all threads. If main_tid > -1, suspend if thread's id != main_tid
  void suspend_threads(const int main_tid = -1);
  void resume_threads(const int main_tid = -1);
  // void inject_code(DWORD thread_id, vecu8 shellcode, u32 sc_entry);

 private:
  void* handle_;
};

Process get_current_process();
}  // namespace process