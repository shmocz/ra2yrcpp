#include "dll_inject.hpp"
#include "instrumentation_client.hpp"
#include "is_context.hpp"
#include "process.hpp"
#include "util_string.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

void ls(std::vector<std::string> args) {
  (void)args;
  auto plist = process::get_process_list();
  for (auto pid : plist) {
    std::cout << process::get_process_name(pid) << "\t" << pid << std::endl;
  }
}

void inject_dll(std::vector<std::string> args) {
  auto path_dll = args[0];
  u32 pid = std::stoi(args[1]);
  process::Process P(pid);
  auto addrs = is_context::get_procaddrs();
  is_context::DLLoader L(addrs.p_LoadLibrary, addrs.p_GetProcAddress, path_dll,
                         "init_iservice");
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  dll_inject::suspend_inject_resume(P.handle(), sc);
#if 0
  auto handle = winutils::open_process(pid);
  dll_inject::suspend_inject_resume(handle, path_dll, path_shellcode);
#endif
}

struct cmd_entry {
  std::string name;
  std::vector<std::string> args;
  std::function<void(std::vector<std::string>)> fn;
};

int main(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);

  std::vector<cmd_entry> cmds = {
      {"inject_dll", {"path_dll", "pid"}, &inject_dll}, {"ls", {}, &ls}};

  std::vector<std::string> keys;
  std::transform(cmds.begin(), cmds.end(), std::back_inserter(keys),
                 [&argv](auto a) { return a.name; });
  std::string scmd = yrclient::join_string(keys, "|");
  std::vector<std::string> qq{"Usage:", argv[0], scmd};
  auto usage = yrclient::join_string(qq, " ");
  if (argc < 2) {
    std::cerr << usage << std::endl;
    return 1;
  }
  auto r = std::find_if(cmds.begin(), cmds.end(),
                        [&argv](auto a) { return argv[1] == a.name; });
  if (r == cmds.end()) {
    std::cerr << usage << std::endl;
    return 1;
  }

  if (args.size() - 2 < r->args.size()) {
    qq[2] = yrclient::join_string(r->args, " ");
    usage = yrclient::join_string(qq, " ");
    std::cerr << usage << std::endl;
    return 1;
  }

  args.erase(args.begin(), args.begin() + 2);
  r->fn(args);

  return 0;
}
