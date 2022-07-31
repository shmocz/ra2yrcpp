#include "ra2yrcppcli.hpp"

using namespace ra2yrcppcli;
using namespace std::chrono_literals;

template <typename T>
std::unique_ptr<T> uptr(T* o) {
  return std::unique_ptr<T>(o);
}

auto get_client(const std::string host, const std::string port) {
  return uptr(new multi_client::AutoPollClient(host, port, 250ms, 40000ms));
}

void show_fields(const google::protobuf::Descriptor* d) {
  for (int i = 0; i < d->field_count(); i++) {
    auto* F = d->field(i);
    fmt::print("i={}, cpp_name={}\n", i, F->cpp_type_name());
  }
}

std::vector<const google::protobuf::Descriptor*> get_messages(
    const google::protobuf::FileDescriptor* d) {
  std::vector<const google::protobuf::Descriptor*> res;
  for (int i = 0; i < d->message_type_count(); i++) {
    res.push_back(d->message_type(i));
  }
  return res;
}

void show_messages(const google::protobuf::FileDescriptor* d) {
  auto msgs = get_messages(d);
  for (int i = 0; i < static_cast<int>(msgs.size()); i++) {
    auto* m = msgs[i];
    fmt::print("i={}, msg={}, fields={}\n", i, m->name(), m->field_count());
    show_fields(m);
  }
}

void desc_info(const google::protobuf::FileDescriptor* d) {
  fmt::print("ptr={}, name={},num messages={}\n", (const void*)d, d->name(),
             d->message_type_count());
  show_messages(d);
}

void set_field(const google::protobuf::Reflection* refl,
               google::protobuf::Message* msg,
               const google::protobuf::FieldDescriptor* field,
               const std::string value) {
  using CppType = google::protobuf::FieldDescriptor::CppType;
  switch (field->cpp_type()) {
    case CppType::CPPTYPE_BOOL:
      refl->SetBool(msg, field, std::stoi(value) != 0);
      break;
    case CppType::CPPTYPE_FLOAT:
      refl->SetDouble(msg, field, std::stof(value));
      break;
    case CppType::CPPTYPE_INT32:
      refl->SetInt32(msg, field, std::stoi(value));
      break;
    case CppType::CPPTYPE_INT64:
      refl->SetInt32(msg, field, std::stol(value));
      break;
    case CppType::CPPTYPE_UINT32:
      refl->SetUInt32(msg, field, std::stoul(value));
      break;
    case CppType::CPPTYPE_UINT64:
      refl->SetUInt64(msg, field, std::stoul(value));
      break;
    case CppType::CPPTYPE_DOUBLE:
      refl->SetDouble(msg, field, std::stod(value));
      break;
    case CppType::CPPTYPE_STRING:
      refl->SetString(msg, field, value);
      break;
    default:
      break;
  }
}

void set_message_field(google::protobuf::Message* m, const std::string key,
                       const std::string value) {
  auto* refl = m->GetReflection();
  auto* desc = m->GetDescriptor();
  auto* fld = desc->FindFieldByName(key);

  set_field(refl, m, fld, value);
}

google::protobuf::Message* create_command_message(
    const std::string name, google::protobuf::DynamicMessageFactory* F,
    const std::map<std::string, std::string> args = {}) {
  auto* pool = google::protobuf::DescriptorPool::generated_pool();
  auto* desc = pool->FindMessageTypeByName(name);
  if (desc == nullptr) {
    throw std::runtime_error(name);
  }
  auto* msg_proto = F->GetPrototype(desc);
  auto* n = msg_proto->New();

  auto* reflection = n->GetReflection();
  auto* args_field = desc->FindFieldByName("args");
  auto* cmd_args = reflection->MutableMessage(n, args_field);
  for (const auto& [k, v] : args) {
    fmt::print(stderr, "set field, cmd_args={}\n",
               reinterpret_cast<void*>(cmd_args));
    set_message_field(cmd_args, k, v);
  }
  return n;
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
      fmt::print("{}\n", m->name());
    }
  }
}

auto parse_kwargs(std::vector<std::string> tokens) {
  std::map<std::string, std::string> res;
  for (const auto& s : tokens) {
    auto pos = s.find("=");
    res[s.substr(0, pos)] = s.substr(pos + 1);
  }
  return res;
}

void send_command(const std::string name, multi_client::AutoPollClient* A,
                  std::vector<std::string> args = {}) {
  auto kw = parse_kwargs(args);
  // NB. factory owns the messages it produces
  google::protobuf::DynamicMessageFactory F;
  auto* msg =
      create_command_message(std::string("yrclient.commands.") + name, &F, kw);

  auto r = A->send_command(*msg);
  fmt::print("{}\n", yrclient::to_json(r));
}

std::string proc_basename(const std::string name) {
  auto pos = name.rfind("\\");
  pos = pos == std::string::npos ? 0u : pos;
  return name.substr(pos + 1);
}

unsigned get_process_id(const std::string name) {
  auto plist = process::get_process_list();
  for (auto pid : plist) {
    auto n = proc_basename(process::get_process_name(pid));
    if (n == name) {
      return pid;
    }
  }
  return 0;
}

void inject_dll(unsigned pid, const std::string path_dll,
                IServiceOptions options, DLLInjectOptions dll) {
  using namespace std::chrono_literals;
  if (pid == 0u) {
    util::call_until(std::chrono::milliseconds(
                         dll.wait_process > 0 ? dll.wait_process : UINT_MAX),
                     500ms, [&]() {
                       return (pid = get_process_id(dll.process_name)) == 0u;
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

void easy_setup(const std::string path_dll, IServiceOptions iservice,
                DLLInjectOptions dll) {
  inject_dll(0u, path_dll, iservice, dll);
  constexpr std::array<const char*, 2> A{"CreateHooks", "CreateCallbacks"};
  int tries = 3;
  auto client = get_client(iservice.host, std::to_string(iservice.port));

  for (auto& s : A) {
    while (tries > 0) {
      try {
        fmt::print(stderr, "sending  {}\n", s);
        send_command(s, client.get());
        fmt::print(stderr, "send cmd ok\n");
        break;
      } catch (const std::exception& e) {
        if (tries <= 0) {
          throw e;
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
      .default_value(std::string("127.0.0.1"));
  A.add_argument("-l", "--list-commands")
      .help("list available commands")
      .default_value(false)
      .implicit_value(true);
  A.add_argument("-n", "--name").help("command to execute on the server");
  A.add_argument("-a", "--args")
      .nargs(argparse::nargs_pattern::any)
      .help("command args");
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
    send_command(A.get("name"), client.get(),
                 A.get<std::vector<std::string>>("args"));
    return 0;
  }

  if (A.get<bool>("--easy-setup")) {
    easy_setup(A.get("--dll-file"), opts, opts_dll);
    return 0;
  }
  std::cerr << A << std::endl;
}
