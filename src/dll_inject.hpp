#pragma once

#include <string>
#include "process.hpp"
#include "types.h"

namespace dll_inject {

typedef void* handle_t;

void inject_code(process::Process* P, int thread_id, vecu8 shellcode);
/// Inject DLL from file path_dll to external process indicated by ex_handle,
/// using payload shellcode. TODO: we probably don't need handle, just the
/// process id.
void suspend_inject_resume(handle_t ex_handle, const std::string path_dll,
                           vecu8 shellcode);
};  // namespace dll_inject
