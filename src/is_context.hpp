#pragma once
#include "constants.hpp"
#include "instrumentation_service.hpp"
#include "types.h"

#include <xbyak/xbyak.h>

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <string>

namespace dll_inject {
struct DLLInjectOptions;
}

namespace context {
class Context;
}

namespace is_context {
using context::Context;

struct ProcAddrs {
  std::uintptr_t p_LoadLibrary;
  std::uintptr_t p_GetProcAddress;
};

ProcAddrs get_procaddrs();
vecu8 vecu8cstr(std::string s);

void get_procaddr(Xbyak::CodeGenerator* c, void* m, std::string name,
                  const std::uintptr_t p_GetProcAddress);

struct DLLLoader : Xbyak::CodeGenerator {
  struct Options {
    ProcAddrs PA;
    std::string path_dll;
    std::string name_init;
    unsigned int max_clients;
    unsigned int port;
    bool indirect;
    bool no_init_hooks;
  };

  explicit DLLLoader(DLLLoader::Options o);
};

/// Create InstrumentationService instance and add both builtin commands and YR
/// specific commands.
ra2yrcpp::InstrumentationService* make_is(
    ra2yrcpp::InstrumentationService::Options O,
    std::function<std::string(ra2yrcpp::InstrumentationService*)> on_shutdown =
        nullptr);

///
/// Inject ra2yrcpp DLL to target process.
///
void inject_dll(unsigned pid, std::string path_dll,
                ra2yrcpp::InstrumentationService::Options o,
                dll_inject::DLLInjectOptions dll);

void* get_context(ra2yrcpp::InstrumentationService::Options O);

// Global ra2yrcpp instance
struct RA2YRCPP {
  ra2yrcpp::InstrumentationService::Options o;
  ra2yrcpp::InstrumentationService* service_;
  std::map<std::uintptr_t, hook::Hook> hooks_;
  void create_all_hooks();
  void create_all_hooks(char* hooks_section, std::size_t section_size,
                        void* dll_handle);
  void create_hook(hook::HookEntry h, hook::hook_fn f);
  void start_service();
  // Get global instance
  static RA2YRCPP* get();
};

const DLLLoader::Options default_options{
    {0U, 0U},         cfg::DLL_NAME, cfg::INIT_NAME, cfg::MAX_CLIENTS,
    cfg::SERVER_PORT, false,         false};

};  // namespace is_context
