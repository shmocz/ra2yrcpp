#pragma once
#include "commands_builtin.hpp"
#include "commands_yr.hpp"
#include "config.hpp"
#include "context.hpp"
#include "dll_inject.hpp"
#include "instrumentation_service.hpp"
#include "x86.hpp"

#include <xbyak/xbyak.h>

#include <algorithm>
#include <string>
#include <vector>

namespace is_context {
using context::Context;

u32 get_proc_address(const std::string addr);

struct DLLInjectOptions {
  unsigned delay_pre;
  unsigned delay_post;
  unsigned wait_process;
  std::string process_name;
  bool force;

  DLLInjectOptions()
      : delay_pre(0u),
        delay_post(0u),
        wait_process(0u),
        process_name(""),
        force(false) {}
};

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

yrclient::InstrumentationService* make_is(
    const unsigned int max_clients, const unsigned int port,
    std::function<std::string(yrclient::InstrumentationService*)> on_shutdown =
        nullptr);

void inject_dll(unsigned pid, const std::string path_dll,
                yrclient::InstrumentationService::IServiceOptions options,
                DLLInjectOptions dll);

};  // namespace is_context
