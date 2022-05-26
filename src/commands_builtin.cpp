#include "commands_builtin.hpp"

using namespace commands_builtin;
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

void save_command_result(command::Command* c, google::protobuf::Message* m) {
  auto* r = new yrclient::NewResult();
  r->mutable_body()->PackFrom(*m);
  c->set_result(yrclient::as<void*>(r));
}

void commands_builtin::command_deleter(command::Command* c) {
  if (c->result()) {
    auto* M = yrclient::as<yrclient::NewResult*>(c->result());
    delete M;
  }
}

// TODO: if we could somehow get rid of the message specialization boilerplate,
// e.g. with some template spells or protobuf's reflection features, that would
// be awesome
static std::map<std::string, command::Command::handler_t> commands_new = {
    {"StoreValue",
     [](command::Command* c) {
       ISCommand<yrclient::StoreValue> Q(c);
       // NB: ensure correct radix
       auto& a = Q.args();
       auto v = new vecu8(a.value().begin(), a.value().end());
       Q.I()->store_value(a.key(), v);
       Q.set_result(a.value());
       // Encode result to NewResult message and put to result queue
       // TODO: could do this in dtor?
       Q.save_command_result();
     }},
    {"GetValue",
     [](command::Command* c) {
       ISCommand<yrclient::GetValue> Q(c);
       // NB: ensure correct radix
       auto val = yrclient::as<vecu8*>(Q.I()->get_value(Q.args().key()));
       Q.set_result(yrclient::to_string(*val));
       Q.save_command_result();
     }},
    {"HookableCommand",
     [](command::Command* c) {
       static TestProgram t;
       auto t_addr = t.get_code();
       t_addr(3, 3);
       ISCommand<yrclient::HookableCommand> Q(c);
       // yrclient::HookableCommand::Result res;
       auto res = Q.result().mutable_result();
       res->set_address_test_function(reinterpret_cast<u32>(t_addr));
       res->set_address_test_callback(reinterpret_cast<u32>(&test_cb));
       res->set_code_size(t.entry_size);
       Q.save_command_result();
     }},
    {"InstallHook",
     [](command::Command* c) {
       ISCommand<yrclient::InstallHook> Q(c);
       auto& a = Q.args();
       Q.I()->create_hook(a.name(), reinterpret_cast<u8*>(a.address()),
                          a.code_length());
       Q.save_command_result();
     }},
    {"AddCallback", [](command::Command* c) {
       ISCommand<yrclient::AddCallback> Q(c);
       auto& a = Q.args();
       // TODO: create proper ctors for HookCallback
       Q.I()
           ->hooks()
           .at(reinterpret_cast<u8*>(a.hook_address()))
           .add_callback(hook::Hook::HookCallback{
               reinterpret_cast<hook::Hook::hook_cb_t>(a.callback_address()),
               Q.I()});
       Q.save_command_result();
     }}};

std::map<std::string, command::Command::handler_t>*
commands_builtin::get_commands() {
  return &commands_new;
}
