#pragma once
#include "config.hpp"
#include "context.hpp"
#include "instrumentation_service.hpp"

#include <xbyak/xbyak.h>

#include <algorithm>
#include <string>
#include <vector>

namespace is_context {
using context::Context;

u32 get_proc_address(const std::string addr);

struct ProcAddrs {
  u32 p_LoadLibrary;
  u32 p_GetProcAddress;
};

ProcAddrs get_procaddrs();
vecu8 vecu8cstr(const std::string s);
void make_is_ctx(Context* c, const unsigned int max_clients = cfg::MAX_CLIENTS,
                 const unsigned int port = cfg::SERVER_PORT);

void get_procaddr(Xbyak::CodeGenerator* c, HMODULE m, const std::string name,
                  const u32 p_GetProcAddress);
struct DLLoader : Xbyak::CodeGenerator {
  DLLoader(u32 p_LoadLibrary, u32 p_GetProcAddress, const std::string path_dll,
           const std::string name_init,
           const unsigned int max_clients = cfg::MAX_CLIENTS,
           const unsigned int port = cfg::SERVER_PORT);
};
};  // namespace is_context
