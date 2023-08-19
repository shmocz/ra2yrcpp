#pragma once
#include "config.hpp"
#include "instrumentation_service.hpp"
#include "types.h"

#include <xbyak/xbyak.h>

#include <functional>
#include <string>

namespace context {
class Context;
}

namespace is_context {
using context::Context;

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

void get_procaddr(Xbyak::CodeGenerator* c, void* m, const std::string name,
                  const u32 p_GetProcAddress);

struct DLLLoader : Xbyak::CodeGenerator {
  DLLLoader(u32 p_LoadLibrary, u32 p_GetProcAddress, const std::string path_dll,
            const std::string name_init,
            const unsigned int max_clients = cfg::MAX_CLIENTS,
            const unsigned int port = cfg::SERVER_PORT,
            const bool indirect = false, const bool no_init_hooks = false);
};

///
/// Create IS instance and add both builtin commands and YR specific commands.
/// FIXME: make this cross platform
///
yrclient::InstrumentationService* make_is(
    yrclient::InstrumentationService::Options O,
    std::function<std::string(yrclient::InstrumentationService*)> on_shutdown =
        nullptr);

///
/// Inject ra2yrcpp DLL to target process.
///
void inject_dll(unsigned pid, const std::string path_dll,
                yrclient::InstrumentationService::Options o,
                DLLInjectOptions dll);

void* get_context(const yrclient::InstrumentationService::Options O);

};  // namespace is_context
