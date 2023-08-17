#include "protocol/protocol.hpp"

#include "asio_utils.hpp"
#include "config.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "multi_client.hpp"
#include "ra2yrcppcli.hpp"
#include "utility/time.hpp"

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <google/protobuf/descriptor.h>

#include <cstdio>

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2yrcppcli;
using namespace std::chrono_literals;

auto get_client(const std::string host, const std::string port) {
  auto opt = multi_client::default_options;
  opt.host = host;
  opt.port = port;
  return std::make_unique<multi_client::AutoPollClient>(
      std::make_shared<ra2yrcpp::asio_utils::IOService>(), opt);
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
  const std::vector<std::string> ss = {"ra2yrproto.commands.CreateCallbacks",
                                       "ra2yrproto.commands.GetSystemState"};
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

void send_and_print(ra2yrproto::Response r) {
  fmt::print("{}\n", yrclient::to_json(r));
}

void easy_setup(const std::string path_dll,
                yrclient::InstrumentationService::Options iservice,
                is_context::DLLInjectOptions dll) {
  inject_dll(0u, path_dll, iservice, dll);
  int tries = 3;
  auto client =
      get_client(iservice.server.host, std::to_string(iservice.server.port));

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
      .default_value(std::string(cfg::DLL_NAME));
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
  A.add_argument("-G", "--generate-dll-loader")
      .default_value(std::string(""))
      .help(
          "Generate x86 code for loading and initializing the library and "
          "write result to destination file");
  A.add_argument("-agp", "--address-GetProcAddr")
      .help("Address of GetProcAddr function")
      .default_value(0u)
      .scan<'x', unsigned>();
  A.add_argument("-all", "--address-LoadLibraryA")
      .help("Address of LoadLibraryA function")
      .default_value(0u)
      .scan<'x', unsigned>();
  A.add_argument("-nia", "--no-indirect-address")
      .help("Don't treat addresses as indirect")
      .implicit_value(false)
      .default_value(true);

  argparse::ArgumentParser record_command("record");
  record_command.add_argument("--mode").default_value("record").help(
      "input file");
  record_command.add_argument("gzip").help("process gzip compressed file");
  record_command.add_argument("input-file").help("input file");

  A.add_subparser(record_command);

  A.parse_args(argc, argv);

  if (A.is_subcommand_used("record")) {
    const std::string input = record_command.get<std::string>("-i");
    auto mode = record_command.get<std::string>("mode");
    if (mode == "record") {
      yrclient::dump_messages(input, ra2yrproto::ra2yr::GameState());
    } else if (mode == "traffic") {
      yrclient::dump_messages(input, ra2yrproto::ra2yr::TunnelPacket());
    }
  }

  if (A.get<bool>("list-commands")) {
    list_commands();
    return 1;
  }

  yrclient::InstrumentationService::Options opts{};
  opts.server.max_connections = A.get<unsigned>("--max-clients");
  opts.server.port = A.get<unsigned>("--port");
  opts.server.host = A.get("--destination");

  is_context::DLLInjectOptions opts_dll;
  opts_dll.delay_post = A.get<unsigned>("--delay-post");
  opts_dll.delay_pre = A.get<unsigned>("--delay-pre");
  opts_dll.wait_process = A.get<unsigned>("--wait");
  opts_dll.process_name = A.get("--process-name");
  opts_dll.force = A.get<bool>("--force");

  if (A.is_used("--game-pid")) {
    inject_dll(A.get<unsigned>("--game-pid"), A.get("--dll-file"), opts,
               opts_dll);
    return 0;
  }

  if (A.is_used("--name")) {
    auto client =
        get_client(opts.server.host, std::to_string(opts.server.port));
    try {
      send_and_print(ra2yrcppcli::send_command(client.get(), A.get("name"),
                                               A.get("args")));
    } catch (const std::exception& e) {
      std::cerr << "failed to send command: " << e.what() << std::endl;
      std::cerr << "use -l to see available commands" << std::endl;
    }
    return 0;
  }

  if (A.is_used("--generate-dll-loader")) {
    // TODO: global constants
    is_context::ProcAddrs PA;
    if (A.is_used("--address-GetProcAddr")) {
      PA.p_GetProcAddress = A.get<unsigned>("--address-GetProcAddr");
    } else {
      PA.p_GetProcAddress = is_context::get_proc_address("GetProcAddress");
    }
    if (A.is_used("--address-LoadLibraryA")) {
      PA.p_LoadLibrary = A.get<unsigned>("--address-LoadLibraryA");
    } else {
      PA.p_LoadLibrary = is_context::get_proc_address("LoadLibraryA");
    }

    is_context::DLLLoader L(PA.p_LoadLibrary, PA.p_GetProcAddress,
                            cfg::DLL_NAME, cfg::INIT_NAME, cfg::MAX_CLIENTS,
                            cfg::SERVER_PORT,
                            A.get<bool>("--no-indirect-address"), false);
    auto p = L.getCode<void __cdecl (*)(void)>();
    std::ofstream os(A.get("--generate-dll-loader"), std::ios::binary);
    os << std::string(reinterpret_cast<char*>(p), L.getSize());
    return 0;
  }

  if (A.get<bool>("--easy-setup")) {
    easy_setup(A.get("--dll-file"), opts, opts_dll);
    return 0;
  }
  std::cerr << A << std::endl;
}
