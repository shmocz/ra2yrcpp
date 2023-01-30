#include "is_context.hpp"

using namespace std::chrono_literals;

using namespace is_context;
using x86::bytes_to_stack;

u32 is_context::get_proc_address(const std::string addr) {
  return reinterpret_cast<u32>(
      GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), addr.c_str()));
}

// TODO(shmocz): utilize in tests
ProcAddrs is_context::get_procaddrs() {
  ProcAddrs A;
  A.p_LoadLibrary = get_proc_address("LoadLibraryA");
  A.p_GetProcAddress = get_proc_address("GetProcAddress");
  return A;
}

vecu8 is_context::vecu8cstr(const std::string s) {
  vecu8 r(s.begin(), s.end());
  r.push_back('\0');
  return r;
}

void is_context::make_is_ctx(Context* c, const unsigned int max_clients,
                             const unsigned int port, const unsigned ws_port) {
  yrclient::InstrumentationService::IServiceOptions O{max_clients, port,
                                                      ws_port, ""};
  auto* I = is_context::make_is(O, [c](auto* X) {
    (void)X;
    return c->on_signal();
  });
  c->data() = reinterpret_cast<void*>(I);
  c->deleter() = [](Context* ctx) {
    delete reinterpret_cast<decltype(I)>(ctx->data());
  };
  c->set_on_signal([](Context* ctx) {
    auto* II = reinterpret_cast<decltype(I)>(ctx->data());
    return II->on_shutdown_(II);
  });
}

// TODO(shmocz): rename
// FIXME: Use IServiceOptions
DLLoader::DLLoader(u32 p_LoadLibrary, u32 p_GetProcAddress,
                   const std::string path_dll, const std::string name_init,
                   const unsigned int max_clients, const unsigned int port,
                   const unsigned int ws_port) {
  vecu8 v1(path_dll.begin(), path_dll.end());
  v1.push_back(0x0);
  vecu8 v2(name_init.begin(), name_init.end());
  v2.push_back(0x0);
  x86::save_regs(this);
  // Call LoadLibrary
  push(ebp);
  mov(ebp, esp);
  auto sz = bytes_to_stack(this, v1);
  lea(eax, ptr[ebp - sz]);
  push(eax);
  mov(eax, p_LoadLibrary);
  call(eax);  // TODO(shmocz): handle errors
  // restore stack
  mov(esp, ebp);
  push(eax);                      // save init fn address
  sz = bytes_to_stack(this, v2);  // lpProcName
  lea(eax, ptr[ebp - sz - 0x4]);
  push(eax);  // &lpProcName
  mov(eax, ptr[ebp - 0x4]);
  push(eax);  // hModule
  mov(eax, p_GetProcAddress);
  call(eax);  // GetProcAddress(hModule, lpProcName)
  // Call init routine
  push(ws_port);
  push(port);
  push(max_clients);
  call(eax);
  // Restore registers
  mov(esp, ebp);
  pop(ebp);
  x86::restore_regs(this);
  // FIXME: better separation
  if (port > 0U) {
    ret();
  }
}

void is_context::get_procaddr(Xbyak::CodeGenerator* c, HMODULE m,
                              const std::string name,
                              const u32 p_GetProcAddress) {
  using namespace Xbyak::util;
  vecu8 n = vecu8cstr(name);
  c->push(ebp);
  c->mov(ebp, esp);
  auto sz = bytes_to_stack(c, n);
  c->lea(eax, ptr[ebp - sz]);
  c->push(eax);
  c->push(reinterpret_cast<u32>(m));
  c->mov(eax, p_GetProcAddress);
  c->call(eax);
  // restore stack
  c->mov(esp, ebp);
  c->pop(ebp);
  c->ret();
}

static void add_builtin_commands(yrclient::InstrumentationService* I) {
  for (auto& [name, fn] : commands_yr::get_commands()) {
    I->add_command(name, fn);
  }

  for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
    I->add_command(name, fn);
  }
}

yrclient::InstrumentationService* is_context::make_is(
    yrclient::InstrumentationService::IServiceOptions O,
    std::function<std::string(yrclient::InstrumentationService*)> on_shutdown) {
  auto* I = new yrclient::InstrumentationService(O, on_shutdown);
  add_builtin_commands(I);
  // If ws_port is defined, then assume we are in gamemd process and create
  // hooks/callbacks
  // FIXME: explicit setting for this
  if (I->opts().ws_port > 0U) {
    multi_client::AutoPollClient C(O.host, std::to_string(O.port), 5000ms,
                                   5000ms);
    ra2yrproto::commands::CreateHooks C1;
    (void)C.send_command(C1);
    ra2yrproto::commands::CreateCallbacks C2;
    (void)C.send_command(C2);
  }
  return I;
}

void is_context::inject_dll(
    unsigned pid, const std::string path_dll,
    yrclient::InstrumentationService::IServiceOptions options,
    DLLInjectOptions dll) {
  using namespace std::chrono_literals;
  if (pid == 0u) {
    util::call_until(std::chrono::milliseconds(
                         dll.wait_process > 0 ? dll.wait_process : UINT_MAX),
                     500ms, [&]() {
                       return (pid = process::get_pid(dll.process_name)) == 0u;
                     });
    if (pid == 0u) {
      throw std::runtime_error("gamemd process not found");
    }
  }
  process::Process P(pid);
  auto modules = P.list_loaded_modules();
  bool is_loaded = std::find_if(modules.begin(), modules.end(), [&](auto& a) {
                     return a.find(path_dll) != std::string::npos;
                   }) != modules.end();
  if (is_loaded && !dll.force) {
    fmt::print(stderr, "DLL {} is already loaded. Not forcing another load.\n",
               path_dll);
    return;
  }
  auto addrs = is_context::get_procaddrs();
  fmt::print(stderr, "pid={},p_load={},p_getproc={},port={}\n", pid,
             reinterpret_cast<void*>(addrs.p_LoadLibrary),
             reinterpret_cast<void*>(addrs.p_GetProcAddress), options.port);
  is_context::DLLoader L(addrs.p_LoadLibrary, addrs.p_GetProcAddress, path_dll,
                         "init_iservice", options.max_clients, options.port);
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  dll_inject::suspend_inject_resume(
      P.handle(), sc, std::chrono::milliseconds(dll.delay_post), 1000ms,
      std::chrono::milliseconds(dll.delay_pre));
}
