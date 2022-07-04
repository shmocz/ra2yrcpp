#include "commands_builtin.hpp"

using namespace yrclient::commands;
using util_command::ISCommand;

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

void test_cb(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  (void)state;
  auto I = static_cast<yrclient::InstrumentationService*>(data);
  std::string s("0xbeefdead");
  I->store_value("test_key", new vecu8(s.begin(), s.end()));
}

void yrclient::commands_builtin::command_deleter(command::Command* c) {
  if (c->result()) {
    auto* M = yrclient::as<yrclient::CommandResult*>(c->result());
    delete M;
  }
}

// TODO: if we could somehow get rid of the message specialization boilerplate,
// e.g. with some template spells or protobuf's reflection features, that would
// be awesome
static std::map<std::string, command::Command::handler_t> commands_new = {
    {"GetSystemState",
     [](command::Command* c) {
       ISCommand<GetSystemState> Q(c);
       auto* state = Q.command_data().mutable_result()->mutable_state();
       for (auto& c : Q.I()->server().connections()) {
         auto* conn = state->add_connections();
         conn->set_socket_id(c->c().socket());
         auto dur =
             std::chrono::duration<double>(c->timestamp().time_since_epoch());
         conn->set_timestamp(dur.count());
       }
       auto& rq = Q.I()->cmd_manager().results_queue();
       for (const auto& [k, v] : rq) {
         state->add_queues()->set_queue_id(k);
       }
     }},
    {"StoreValue",
     [](command::Command* c) {
       ISCommand<StoreValue> Q(c);
       // NB: ensure correct radix
       auto& a = Q.args();
       auto v = new vecu8(a.value().begin(), a.value().end());
       auto [lk, s] = Q.I()->aq_storage();
       Q.I()->store_value(a.key(), v);
       Q.set_result(a.value());
     }},
    {"GetValue",
     [](command::Command* c) {
       ISCommand<GetValue> Q(c);
       // NB: ensure correct radix
       // FIXME: proper locking
       auto val = yrclient::as<vecu8*>(Q.I()->get_value(Q.args().key()));
       Q.set_result(yrclient::to_string(*val));
     }},
    {"HookableCommand",
     [](command::Command* c) {
       static TestProgram t;
       auto t_addr = t.get_code();
       t_addr(3, 3);
       ISCommand<HookableCommand> Q(c);
       // yrclient::HookableCommand::Result res;
       auto res = Q.command_data().mutable_result();
       res->set_address_test_function(reinterpret_cast<u32>(t_addr));
       res->set_address_test_callback(reinterpret_cast<u32>(&test_cb));
       res->set_code_size(t.entry_size);
     }},
    {"InstallHook",
     [](command::Command* c) {
       ISCommand<InstallHook> Q(c);
       auto& a = Q.args();
       Q.I()->create_hook(a.name(), reinterpret_cast<u8*>(a.address()),
                          a.code_length());
     }},
    {"AddCallback", [](command::Command* c) {
       ISCommand<AddCallback> Q(c);
       auto& a = Q.args();
       // TODO: create proper ctors for HookCallback
       Q.I()
           ->hooks()
           .at(reinterpret_cast<u8*>(a.hook_address()))
           .add_callback(hook::Hook::HookCallback{
               reinterpret_cast<hook::Hook::hook_cb_t>(a.callback_address()),
               Q.I()});
     }}};

std::map<std::string, command::Command::handler_t>*
yrclient::commands_builtin::get_commands() {
  return &commands_new;
}
