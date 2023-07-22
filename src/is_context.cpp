#include "is_context.hpp"

#include "command/command.hpp"
#include "commands_builtin.hpp"
#include "commands_yr.hpp"
#include "dll_inject.hpp"
#include "hooks_yr.hpp"
#include "util_string.hpp"
#include "x86.hpp"

#include <map>
#include <memory>

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
                             const unsigned int port, const unsigned ws_port,
                             bool no_init_hooks) {
  // FIXME: the no init  flag
  yrclient::InstrumentationService::IServiceOptions O{
      max_clients, port, ws_port, cfg::SERVER_ADDRESS, no_init_hooks};
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
DLLLoader::DLLLoader(u32 p_LoadLibrary, u32 p_GetProcAddress,
                     const std::string path_dll, const std::string name_init,
                     const unsigned int max_clients, const unsigned int port,
                     const unsigned int ws_port, const bool indirect,
                     const bool no_init_hooks) {
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
  if (indirect) {
    mov(eax, ptr[p_LoadLibrary]);
  } else {
    mov(eax, p_LoadLibrary);
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
  if (indirect) {
    mov(eax, ptr[p_GetProcAddress]);
  } else {
    mov(eax, p_GetProcAddress);
  }
  call(eax);  // GetProcAddress(hModule, lpProcName)
  // Call init routine
  push(static_cast<u32>(no_init_hooks));
  push(ws_port);
  push(port);
  push(max_clients);
  call(eax);
  // Restore registers
  mov(esp, ebp);
  pop(ebp);
  x86::restore_regs(this);
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

// FIXME: copypaste code
static void handle_cmd(yrclient::InstrumentationService* I,
                       ra2yrproto::Command cmd) {
  // TODO: reduce amount of copies we make
  auto client_cmd = cmd.command();
  // schedule command execution
  auto is_args = new yrclient::ISArgs;
  is_args->I = I;
  is_args->M.CopyFrom(client_cmd);

  // Get trailing portion of protobuf type url
  auto name = yrclient::split_string(client_cmd.type_url(), "/").back();

  auto c = std::shared_ptr<command::Command>(
      I->cmd_manager().factory().make_command(
          name,
          std::unique_ptr<void, void (*)(void*)>(
              is_args,
              [](auto d) { delete reinterpret_cast<yrclient::ISArgs*>(d); }),
          0U),
      [](auto* a) { delete a; });
  c->discard_result().store(true);
  I->cmd_manager().enqueue_command(c);
  // FIXME: error check
  c->result_code().wait_pred(
      [](auto v) { return v != command::ResultCode::NONE; });
  // write status back
}

yrclient::InstrumentationService* is_context::make_is(
    yrclient::InstrumentationService::IServiceOptions O,
    std::function<std::string(yrclient::InstrumentationService*)> on_shutdown) {
  // FIXME: ensure that initialization has been completed before starting the
  // tcp server
  auto* I = yrclient::InstrumentationService::create(
      O, nullptr, on_shutdown, [&](auto* t) {
        std::map<std::string, command::Command::handler_t> cmds;
        for (auto& [name, fn] : commands_yr::get_commands()) {
          cmds[name] = fn;
        }

        for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
          cmds[name] = fn;
        }

        for (auto& [name, fn] : cmds) {
          t->add_command(name, fn);
        }
        // If ws_port is defined, then assume we are in gamemd process and
        // create hooks/callbacks
        // FIXME: explicit setting for this

        if (t->opts().ws_port > 0U && !t->opts().no_init_hooks) {
          ra2yrproto::commands::CreateHooks C1;

          C1.mutable_args()->set_no_suspend_threads(true);
          for (const auto& [k, v] : ra2yrcpp::hooks_yr::get_hooks()) {
            auto [p_target, code_size] = hook::get_hook_entry(v);
            auto* H = C1.mutable_args()->add_hooks();
            H->set_address(p_target);
            H->set_name(k);
            H->set_code_length(code_size);
          }

          handle_cmd(t, yrclient::create_command(C1));
          ra2yrproto::commands::CreateCallbacks C2;
          handle_cmd(t, yrclient::create_command(C2));
        }
      });

  return I;
}

void is_context::inject_dll(
    unsigned pid, const std::string path_dll,
    yrclient::InstrumentationService::IServiceOptions options,
    DLLInjectOptions dll) {
  using namespace std::chrono_literals;
  if (pid == 0u) {
    util::call_until(
        duration_t(dll.wait_process > 0 ? dll.wait_process : UINT_MAX), 0.5s,
        [&]() { return (pid = process::get_pid(dll.process_name)) == 0u; });
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
  // FIXME: ws port
  is_context::DLLLoader L(addrs.p_LoadLibrary, addrs.p_GetProcAddress, path_dll,
                          cfg::INIT_NAME, options.max_clients, options.port);
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  dll_inject::suspend_inject_resume(P.handle(), sc, duration_t(dll.delay_post),
                                    1.0s, duration_t(dll.delay_pre));
}

void* is_context::get_context(unsigned int max_clients, unsigned int port,
                              unsigned int ws_port, bool no_init_hooks) {
  network::Init();
  auto* context = new is_context::Context();
  is_context::make_is_ctx(context, max_clients, port, ws_port, no_init_hooks);
  return context;
}
