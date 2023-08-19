#pragma once

#include "types.h"

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace windows_utils {

struct ThreadEntry {
  unsigned long owner_process_id;
  unsigned long thread_id;
};

struct ThreadContext {
  void* handle;
  bool acquired;
  std::size_t size;
  void* data;
  ThreadContext();
  explicit ThreadContext(void* handle);
  ~ThreadContext();
  int save();
  int* get_pgpr(const x86Reg);
};

class ExProcess {
 private:
  const std::string cmdline_;
  const std::string directory_;

 public:
  struct ProcessContext;
  std::unique_ptr<ProcessContext> ctx;

  explicit ExProcess(const std::string cmdline,
                     const std::string directory = "");
  void* handle();
  void join();
  unsigned long pid() const;
  ~ExProcess();
};

void* load_library(const std::string name);
std::uintptr_t get_proc_address(const std::string addr, void* module = nullptr);
std::string get_process_name(const int pid);
void* open_thread(unsigned long access, bool inherit_handle,
                  unsigned long thread_id);
void* allocate_memory(void* handle, const std::size_t size,
                      unsigned long alloc_type, unsigned long alloc_protect);
void* allocate_code(void* handle, const std::size_t size);
std::vector<unsigned long> enum_processes();
std::string getcwd();
std::vector<std::string> list_loaded_modules(void* const handle);
unsigned long suspend_thread(void* handle);
void* get_current_process_handle();
int get_current_tid();
unsigned long resume_thread(void* handle);
void* open_process(unsigned long access, bool inherit, unsigned long pid);
int read_memory(void* handle, void* dest, const void* src,
                const std::size_t size);
int close_handle(void* handle);
int write_memory(void* handle, void* dest, const void* src,
                 const std::size_t size);
int write_memory_local(void* dest, const void* src, const std::size_t size);
unsigned long get_pid(void* handle);
void for_each_thread(std::function<void(ThreadEntry*)> callback);

}  // namespace windows_utils
