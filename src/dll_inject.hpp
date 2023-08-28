#pragma once

#include "types.h"

#include <chrono>
#include <string>

namespace process {
class Process;
}

namespace dll_inject {

namespace {
using namespace std::chrono_literals;
}

typedef void* handle_t;

struct DLLInjectOptions {
  duration_t delay_pre_suspend;
  duration_t delay_post_suspend;
  duration_t delay_pre_resume;
  duration_t wait_process;
  duration_t delay_post_inject;
  std::string process_name;
  bool force;

  DLLInjectOptions()
      : delay_pre_suspend(1.0s),
        delay_post_suspend(1.0s),
        delay_pre_resume(1.0s),
        wait_process(0.0s),
        delay_post_inject(1.0s),
        process_name(""),
        force(false) {}
};

namespace {

using namespace std::chrono_literals;
};

void inject_code(process::Process* P, int thread_id, vecu8 shellcode);
/// Inject DLL from file path_dll to external process indicated by ex_handle,
/// using payload shellcode. TODO: we probably don't need handle, just the
/// process id.
/// @exception std::runtime_error on failure
void suspend_inject_resume(handle_t ex_handle, vecu8 shellcode,
                           const DLLInjectOptions o);
};  // namespace dll_inject
