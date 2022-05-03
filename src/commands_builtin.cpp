#include "commands_builtin.hpp"
#include "command_manager.hpp"
#include "util_string.hpp"
#include "instrumentation_service.hpp"
#include <regex>
#include <tuple>

using namespace commands_builtin;

std::tuple<yrclient::InstrumentationService*, std::vector<std::string>, void*>
get_args(yrclient::IServiceArgs args) {
  return std::make_tuple(args.I, yrclient::split_string(*args.args),
                         args.result);
}

u32 parse_address(const std::string s) {
  std::regex re("0x[0-9a-fA-F]+");
  if (!std::regex_match(s, re)) {
    throw std::runtime_error("invalid address");
  }
  return std::stoul(s, nullptr, 16);
}

/// Adds two unsigned integers, and returns result in EAX
struct TestProgram : Xbyak::CodeGenerator {
  TestProgram() {
    mov(eax, ptr[esp + 0x4]);
    add(eax, ptr[esp + 0x8]);
    entry_size = getSize();
    ret();
  }
  auto get_code() { return getCode<int __cdecl (*)(const int, const int)>(); }
  size_t entry_size;
};

static volatile int secret_flag = 0xbeefdead;

void test_callback(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  (void)data;
  (void)state;
  secret_flag = 0xdeadbeef;
}

void test_cb(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  (void)state;
  auto I = static_cast<yrclient::InstrumentationService*>(data);
  std::string s("0xbeefdead");
  I->store_value("test_key", std::make_unique<vecu8>(s.begin(), s.end()));
}

static std::map<std::string, yrclient::IServiceCommand> commands = {
    {"install_hook",
     [](yrclient::IServiceArgs args) -> command_manager::CommandResult {
       auto [I, cmd_args, result] = get_args(args);
       std::string name = cmd_args[0];
       std::string address = cmd_args[1];
       const size_t code_length = std::stoi(cmd_args[2]);
       I->create_hook(name, reinterpret_cast<u8*>(parse_address(address)),
                      code_length);
       return std::make_unique<vecu8>();
     }},
    {"add_callback",
     [](auto args) {
       auto [I, cmd_args, result] = get_args(args);
       u8* addr_hook = reinterpret_cast<u8*>(parse_address(cmd_args[0]));
       auto addr_cb =
           reinterpret_cast<hook::Hook::hook_cb_t>(parse_address(cmd_args[1]));
       // TODO: create proper ctors for HookCallback
       I->hooks().at(addr_hook).add_callback(
           hook::Hook::HookCallback{addr_cb, I});
       return std::make_unique<vecu8>();
     }},
    {"store_value",
     [](auto args) {
       auto [I, cmd_args, result] = get_args(args);
       std::string key = cmd_args[0];
       std::string value = cmd_args[1];
       I->store_value(key, std::make_unique<vecu8>(value.begin(), value.end()));
       return std::make_unique<vecu8>(value.begin(), value.end());
     }},
    {"get_value",
     [](auto args) {
       auto [I, cmd_args, result] = get_args(args);
       std::string key = cmd_args[0];
       auto v = I->get_value(key);
       return std::make_unique<vecu8>(v->begin(), v->end());
     }},
    {"hookable_command",
     [](auto args) {
       static TestProgram t;
       auto [I, cmd_args, result] = get_args(args);
       auto t_addr = t.get_code();
       t_addr(3, 3);
       auto msg = yrclient::join_string(
           {yrclient::to_hex(reinterpret_cast<u32>(t_addr)),
            std::to_string(t.entry_size),
            yrclient::to_hex(reinterpret_cast<u32>(&test_cb))});
       auto res = std::make_unique<vecu8>();
       res->assign(msg.begin(), msg.end());
       return res;
     }},
    {"test_command", [](yrclient::IServiceArgs args) {
       static TestProgram t;
       auto [I, cmd_args, result] = get_args(args);
       auto t_addr = t.get_code();
       t_addr(5, 2);
       auto msg = yrclient::to_hex(reinterpret_cast<u32>(&secret_flag)) + " " +
                  yrclient::to_hex(reinterpret_cast<u32>(t_addr)) + " " +
                  std::to_string(t.entry_size) + " " +
                  yrclient::to_hex(secret_flag) + " " +
                  yrclient::to_hex(reinterpret_cast<u32>(&test_callback));
       auto res = std::make_unique<vecu8>();
       res->assign(msg.begin(), msg.end());
       return res;
     }}};

std::map<std::string, yrclient::IServiceCommand>*
commands_builtin::get_commands() {
  return &commands;
}
