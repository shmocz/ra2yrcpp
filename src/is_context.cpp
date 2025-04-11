#include "is_context.hpp"

#include "command/is_command.hpp"
#include "commands_builtin.hpp"
#include "commands_game.hpp"
#include "commands_yr.hpp"
#include "constants.hpp"
#include "dll_inject.hpp"
#include "hook.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "process.hpp"
#include "types.h"
#include "utility/time.hpp"
#include "win32/windows_utils.hpp"
#include "x86.hpp"

#include <fmt/core.h>
#include <google/protobuf/message.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

using namespace is_context;
using x86::bytes_to_stack;

ProcAddrs is_context::get_procaddrs() {
  ProcAddrs A;
  A.p_LoadLibrary = windows_utils::get_proc_address("LoadLibraryA");
  A.p_GetProcAddress = windows_utils::get_proc_address("GetProcAddress");
  return A;
}

vecu8 is_context::vecu8cstr(std::string s) {
  vecu8 r(s.begin(), s.end());
  r.push_back('\0');
  return r;
}

DLLLoader::DLLLoader(DLLLoader::Options o) {
  vecu8 v1(o.path_dll.begin(), o.path_dll.end());
  v1.push_back(0x0);
  vecu8 v2(o.name_init.begin(), o.name_init.end());
  v2.push_back(0x0);
  x86::save_regs(this);
  // Call LoadLibrary
  push(ebp);
  mov(ebp, esp);
  auto sz = bytes_to_stack(this, v1);
  lea(eax, ptr[ebp - sz]);
  push(eax);
  if (o.indirect) {
    mov(eax, ptr[o.PA.p_LoadLibrary]);
  } else {
    mov(eax, o.PA.p_LoadLibrary);
  }
  call(eax);  // TODO(shmocz): handle errors
  // restore stack
  mov(esp, ebp);
  push(eax);                      // save init fn address
  sz = bytes_to_stack(this, v2);  // lpProcName
  lea(eax, ptr[ebp - sz - 0x4]);
  push(eax);  // &lpProcName
  mov(eax, ptr[ebp - 0x4]);
  push(eax);  // hModule
  if (o.indirect) {
    mov(eax, ptr[o.PA.p_GetProcAddress]);
  } else {
    mov(eax, o.PA.p_GetProcAddress);
  }
  call(eax);  // GetProcAddress(hModule, lpProcName)
  // Call init routine
  push(static_cast<u32>(o.no_init_hooks));
  push(o.port);
  push(o.max_clients);
  call(eax);
  // Restore registers
  mov(esp, ebp);
  pop(ebp);
  x86::restore_regs(this);
}

void is_context::get_procaddr(Xbyak::CodeGenerator* c, void* m,
                              std::string name,
                              const std::uintptr_t p_GetProcAddress) {
  using namespace Xbyak::util;
  vecu8 n = vecu8cstr(name);
  c->push(ebp);
  c->mov(ebp, esp);
  auto sz = bytes_to_stack(c, n);
  c->lea(eax, ptr[ebp - sz]);
  c->push(eax);
  c->push(reinterpret_cast<std::uintptr_t>(m));
  c->mov(eax, p_GetProcAddress);
  c->call(eax);
  // restore stack
  c->mov(esp, ebp);
  c->pop(ebp);
  c->ret();
}

ra2yrcpp::InstrumentationService* is_context::make_is(
    ra2yrcpp::InstrumentationService::Options O,
    std::function<std::string(ra2yrcpp::InstrumentationService*)> on_shutdown) {
  auto cmds = ra2yrcpp::commands_builtin::get_commands();
  for (auto& [name, fn] : commands_yr::get_commands()) {
    cmds[name] = fn;
  }
  for (auto& [name, fn] : ra2yrcpp::commands_game::get_commands()) {
    cmds[name] = fn;
  }
  auto* I = ra2yrcpp::InstrumentationService::create(
      O, std::map<std::string, ra2yrcpp::cmd_t::handler_t>(), on_shutdown,
      [cmds](auto* t) {
        for (auto& [name, fn] : cmds) {
          t->cmd_manager().add_command(name, fn);
        }
      });

  return I;
}

void is_context::inject_dll(unsigned pid, std::string path_dll,
                            ra2yrcpp::InstrumentationService::Options o,
                            dll_inject::DLLInjectOptions dll) {
  using namespace std::chrono_literals;
  if (pid == 0u) {
    util::call_until(
        dll.wait_process > 0.0s ? dll.wait_process : cfg::MAX_TIMEOUT, 0.5s,
        [&]() { return (pid = process::get_pid(dll.process_name)) == 0u; });
    if (pid == 0u) {
      throw std::runtime_error(
          fmt::format("process {} not found", dll.process_name));
    }
  }
  process::Process P(pid);
  const auto modules = P.list_loaded_modules();
  const bool is_loaded =
      std::find_if(modules.begin(), modules.end(), [&](auto& a) {
        return a.find(path_dll) != std::string::npos;
      }) != modules.end();
  if (is_loaded && !dll.force) {
    iprintf("DLL {} is already loaded. Not forcing another load.", path_dll);
    return;
  }
  const DLLLoader::Options o_dll{is_context::get_procaddrs(),
                                 path_dll,
                                 cfg::INIT_NAME,
                                 o.server.max_connections,
                                 o.server.port,
                                 false,
                                 false};
  iprintf("indirect={} pid={},p_load={:#x},p_getproc={:#x},port={}\n",
          o_dll.indirect, pid, o_dll.PA.p_LoadLibrary,
          o_dll.PA.p_GetProcAddress, o_dll.port);
  is_context::DLLLoader L(o_dll);
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  dll_inject::suspend_inject_resume(P.handle(), sc, dll);
}
