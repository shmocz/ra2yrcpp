#include "dll_inject.hpp"
#include "is_context.hpp"
#include "process.hpp"
#include "util_string.hpp"

#include <argparse/argparse.hpp>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;
using std::cout;
using std::endl;

void ls() {
  auto plist = process::get_process_list();
  for (auto pid : plist) {
    std::cout << process::get_process_name(pid) << "\t" << pid << std::endl;
  }
}

void inject_dll(const std::string path_dll, const u32 pid, const u32 delay_post,
                const u32 delay_pre) {
  process::Process P(pid);
  auto addrs = is_context::get_procaddrs();
  is_context::DLLoader L(addrs.p_LoadLibrary, addrs.p_GetProcAddress, path_dll,
                         "init_iservice");
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  cout << yrclient::join_string(
              {"[+] injecting", path_dll, "pid =", std::to_string(pid)})
       << endl;
  dll_inject::suspend_inject_resume(
      P.handle(), sc, std::chrono::milliseconds(delay_post), 1000ms,
      std::chrono::milliseconds(delay_pre));
  cout << "[+] done!" << endl;
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser A(argv[0]);
  A.add_argument("-l").default_value(false).implicit_value(true).help(
      "list processes");
  A.add_argument("-f", "--file").help("path to DLL file");
  A.add_argument("-p", "--pid").help("process id").scan<'u', unsigned>();
  A.add_argument("-dl", "--delay-post")
      .help("delay after suspending threads")
      .default_value(1000u)
      .scan<'u', unsigned>();
  A.add_argument("-dp", "--delay-pre")
      .help("delay before resuming suspended threads")
      .default_value(2000u)
      .scan<'u', unsigned>();

  A.parse_args(argc, argv);
  if (A.get<bool>("-l")) {
    ls();
  } else {
    inject_dll(A.get<std::string>("--file"), A.get<unsigned>("--pid"),
               A.get<unsigned>("--delay-post"), A.get<unsigned>("--delay-pre"));
  }

  return 0;
}
