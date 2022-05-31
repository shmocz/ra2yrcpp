#pragma once

#include "process.hpp"
#include "types.h"
#include "utility/time.hpp"
#include <string>
#include <vector>

namespace dll_inject {

typedef void* handle_t;
namespace {

using namespace std::chrono_literals;
};

void inject_code(process::Process* P, int thread_id, vecu8 shellcode);
/// Inject DLL from file path_dll to external process indicated by ex_handle,
/// using payload shellcode. TODO: we probably don't need handle, just the
/// process id.
void suspend_inject_resume(
    handle_t ex_handle, vecu8 shellcode,
    const std::chrono::milliseconds delay_post_suspend = 1000ms,
    const std::chrono::milliseconds delay_post_inject = 1000ms,
    const std::chrono::milliseconds delay_pre_resume = 1000ms);
};  // namespace dll_inject
