#include "commands_builtin.hpp"

// TODO: rename this to avoid confusion between command manager builtins

using util_command::ISCommand;

using util_command::get_cmd;

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

std::map<std::string, command::Command::handler_t> get_commands_nn() {
  return {
      get_cmd<ra2yrproto::commands::StoreValue>([](auto* Q) {
        // NB: ensure correct radix
        auto& a = Q->args();
        auto v = new vecu8(a.value().begin(), a.value().end());
        auto [lk, s] = Q->I()->aq_storage();
        Q->I()->store_value(a.key(), v);
        Q->set_result(a.value());
      }),
      get_cmd<ra2yrproto::commands::GetSystemState>([](auto* Q) {
        std::lock_guard<std::mutex> lk(Q->I()->server().connections_mut);
        auto* state = Q->command_data().mutable_result()->mutable_state();
        std::vector<server::ConnectionCTX*> active_connections;
        auto& conns = Q->I()->server().connections();
        auto& close = Q->I()->server().close_queue();
        std::transform(conns.begin(), conns.end(),
                       std::back_inserter(active_connections),
                       [](auto& ctx) { return ctx.get(); });
        active_connections.erase(
            std::remove_if(active_connections.begin(), active_connections.end(),
                           [&close](auto* c) {
                             return std::find_if(close.begin(), close.end(),
                                                 [&c](auto* d) {
                                                   return d->socket() ==
                                                          c->c().socket();
                                                 }) != close.end();
                           }),
            active_connections.end());
        for (auto& c : active_connections) {
          auto* conn = state->add_connections();
          conn->set_socket_id(c->c().socket());
          auto dur =
              std::chrono::duration<double>(c->timestamp().time_since_epoch());
          conn->set_timestamp(dur.count());
        }
        // NB. this is safe. We're in cmd manager's main loop thread
        for (const auto& [k, v] : Q->I()->cmd_manager().results_queue()) {
          state->add_queues()->set_queue_id(k);
        }
        state->set_directory(process::getcwd());
      }),
      get_cmd<ra2yrproto::commands::GetValue>([](auto* Q) {
        // NB: ensure correct radix
        // FIXME: proper locking
        auto [lk, s] = Q->I()->aq_storage();
        auto val =
            yrclient::as<vecu8*>(Q->I()->get_value(Q->args().key(), false));
        Q->set_result(yrclient::to_string(*val));
      }),
      get_cmd<ra2yrproto::commands::HookableCommand>([](auto* Q) {
        static TestProgram t;
        auto t_addr = t.get_code();
        t_addr(3, 3);
        // yrclient::HookableCommand::Result res;
        auto res = Q->command_data().mutable_result();
        res->set_address_test_function(reinterpret_cast<u32>(t_addr));
        res->set_address_test_callback(reinterpret_cast<u32>(&test_cb));
        res->set_code_size(t.entry_size);
      }),
      get_cmd<ra2yrproto::commands::InstallHook>([](auto* Q) {
        auto& a = Q->args();
        Q->I()->create_hook(a.name(), reinterpret_cast<u8*>(a.address()),
                            a.code_length());
      }),
      get_cmd<ra2yrproto::commands::AddCallback>([](auto* Q) {
        auto& a = Q->args();
        Q->I()
            ->hooks()
            .at(reinterpret_cast<u8*>(a.hook_address()))
            .add_callback(
                reinterpret_cast<hook::Hook::hook_cb_t>(a.callback_address()),
                Q->I(), "", 0u);
      }),
  };
}

std::map<std::string, command::Command::handler_t>
yrclient::commands_builtin::get_commands() {
  return get_commands_nn();
}
