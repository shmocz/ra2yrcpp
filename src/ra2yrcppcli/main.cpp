#include "protocol/protocol.hpp"

#include "ra2yrcppcli.hpp"

#include <google/protobuf/descriptor.h>

using namespace ra2yrcppcli;
using namespace std::chrono_literals;

template <typename T>
std::unique_ptr<T> uptr(T* o) {
  return std::unique_ptr<T>(o);
}

auto get_client(const std::string host, const std::string port) {
  return uptr(new multi_client::AutoPollClient(host, port, 250ms, 40000ms));
}

std::vector<const google::protobuf::Descriptor*> get_messages(
    const google::protobuf::FileDescriptor* d) {
  std::vector<const google::protobuf::Descriptor*> res;
  for (int i = 0; i < d->message_type_count(); i++) {
    res.push_back(d->message_type(i));
  }
  return res;
}

void list_commands() {
  const std::vector<std::string> ss = {"yrclient.commands.CreateCallbacks",
                                       "yrclient.commands.GetSystemState"};
  auto* pool = google::protobuf::DescriptorPool::generated_pool();
  for (const auto& s : ss) {
    auto* p = pool->FindMessageTypeByName(s);
    if (p == nullptr) {
      throw std::runtime_error("null descriptor");
    }
    auto* fd = p->file();
    auto msgs = get_messages(fd);
    for (auto* m : msgs) {
      fmt::print("{}\n", m->full_name());
    }
  }
}

void send_and_print(yrclient::Response r) {
  fmt::print("{}\n", yrclient::to_json(r));
}

void easy_setup(const std::string path_dll, IServiceOptions iservice,
                DLLInjectOptions dll) {
  inject_dll(0u, path_dll, iservice, dll);
  int tries = 3;
  auto client = get_client(iservice.host, std::to_string(iservice.port));

  for (auto& s : INIT_COMMANDS) {
    while (tries > 0) {
      try {
        fmt::print(stderr, "sending  {}\n", s);
        send_and_print(ra2yrcppcli::send_command(client.get(), s));
        fmt::print(stderr, "send cmd ok\n");
        break;
      } catch (const std::exception& e) {
        if (tries <= 1) {
          throw;
        }
        fmt::print(stderr, "error: {}, tries={}\n", e.what(), tries);
        tries--;
        util::sleep_ms(2000);
      }
    }
  }
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser A(argv[0]);

  A.add_argument("-p", "--port")
      .help("port")
      .default_value(cfg::SERVER_PORT)
      .scan<'u', unsigned>();
  A.add_argument("-d", "--destination")
      .help("host")
      .default_value(std::string(cfg::SERVER_ADDRESS));
  A.add_argument("-l", "--list-commands")
      .help("list available commands")
      .default_value(false)
      .implicit_value(true);
  A.add_argument("-n", "--name").help("command to execute on the server");
  A.add_argument("-a", "--args")
      .default_value(std::string(""))
      .help("command args in json");
  A.add_argument("-g", "--game-pid")
      .help(
          "Inject DLL into gamemd-spawn.exe process with the given PID. Set 0 "
          "to autodetect")
      .default_value(0u)
      .scan<'u', unsigned>();
  A.add_argument("-m", "--max-clients")
      .help("Max clients in server")
      .default_value(cfg::MAX_CLIENTS)
      .scan<'u', unsigned>();
  A.add_argument("-f", "--force")
      .help("Force DLL injection even if module is already loaded")
      .implicit_value(true)
      .default_value(false);
  A.add_argument("-D", "--dll-file")
      .help("Name of DLL to be injected")
      .default_value(std::string("libyrclient.dll"));
  A.add_argument("-N", "--process-name")
      .help("target process name")
      .default_value(std::string("gamemd-spawn.exe"));
  A.add_argument("-dl", "--delay-post")
      .help("delay after suspending threads")
      .default_value(1000u)
      .scan<'u', unsigned>();
  A.add_argument("-dp", "--delay-pre")
      .help("delay before resuming suspended threads")
      .default_value(2000u)
      .scan<'u', unsigned>();
  A.add_argument("-w", "--wait")
      .help(
          "Milliseconds to to wait for gamemd process to show up. If 0 wait "
          "indefinitely")
      .default_value(0u)
      .scan<'u', unsigned>();
  A.add_argument("-e", "--easy-setup")
      .help("Automatically inject the DLL and run initialization commands")
      .implicit_value(true)
      .default_value(true);

  A.parse_args(argc, argv);

  if (A.get<bool>("list-commands")) {
    list_commands();
    return 1;
  }

  IServiceOptions opts;
  opts.max_clients = A.get<unsigned>("--max-clients");
  opts.port = A.get<unsigned>("--port");
  opts.host = A.get("--destination");

  DLLInjectOptions opts_dll;
  opts_dll.delay_post = A.get<unsigned>("--delay-post");
  opts_dll.delay_pre = A.get<unsigned>("--delay-pre");
  opts_dll.wait_process = A.get<unsigned>("--wait");
  opts_dll.process_name = A.get("--process-name");
  opts_dll.force = A.get<bool>("--force");
  network::Init();

  if (A.is_used("--game-pid")) {
    inject_dll(A.get<unsigned>("--game-pid"), A.get("--dll-file"), opts,
               opts_dll);
    return 0;
  }

  if (A.is_used("--name")) {
    auto client = get_client(opts.host, std::to_string(opts.port));
    send_and_print(
        ra2yrcppcli::send_command(client.get(), A.get("name"), A.get("args")));
    return 0;
  }

  if (A.get<bool>("--easy-setup")) {
    easy_setup(A.get("--dll-file"), opts, opts_dll);
    return 0;
  }
  std::cerr << A << std::endl;
}
