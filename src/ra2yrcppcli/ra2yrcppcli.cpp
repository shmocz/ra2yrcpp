#include "ra2yrcppcli/ra2yrcppcli.hpp"

void ra2yrcppcli::inject_dll(unsigned pid, const std::string path_dll,
                             IServiceOptions options, DLLInjectOptions dll) {
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
  fmt::print(stderr, "p_load={},p_getproc={}\n",
             reinterpret_cast<void*>(addrs.p_LoadLibrary),
             reinterpret_cast<void*>(addrs.p_GetProcAddress));
  is_context::DLLoader L(addrs.p_LoadLibrary, addrs.p_GetProcAddress, path_dll,
                         "init_iservice", options.max_clients, options.port);
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  dll_inject::suspend_inject_resume(
      P.handle(), sc, std::chrono::milliseconds(dll.delay_post), 1000ms,
      std::chrono::milliseconds(dll.delay_pre));
}

yrclient::Response ra2yrcppcli::send_command(const std::string name,
                                             multi_client::AutoPollClient* A,
                                             std::vector<std::string> args) {
  auto kw = parse_kwargs(args);
  // NB. factory owns the messages it produces
  google::protobuf::DynamicMessageFactory F;
  auto* msg = yrclient::create_command_message(
      std::string("yrclient.commands.") + name, &F, kw);

  auto r = A->send_command(*msg);
  return r;
}

std::map<std::string, std::string> ra2yrcppcli::parse_kwargs(
    std::vector<std::string> tokens) {
  std::map<std::string, std::string> res;
  for (const auto& s : tokens) {
    auto pos = s.find("=");
    res[s.substr(0, pos)] = s.substr(pos + 1);
  }
  return res;
}