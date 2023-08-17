#include "commands_builtin.hpp"

#include "ra2yrproto/commands_builtin.pb.h"

#include "asio_utils.hpp"
#include "hook.hpp"
#include "instrumentation_service.hpp"
#include "process.hpp"
#include "types.h"
#include "util_command.hpp"
#include "util_string.hpp"

#include <xbyak/xbyak.h>

#include <cstddef>
#include <utility>
#include <vector>

using util_command::get_cmd;

/// Adds two unsigned integers, and returns result in EAX
struct TestProgram : Xbyak::CodeGenerator {
  TestProgram() {
    mov(eax, ptr[esp + 1 * 0x4]);
    add(eax, ptr[esp + 2 * 0x4]);
    entry_size = getSize();
    ret();
  }

  auto get_code() { return getCode<i32 __cdecl (*)(const i32, const i32)>(); }

  size_t entry_size;
};

static void test_cb(hook::Hook*, void* data, X86Regs*) {
  auto I = static_cast<yrclient::InstrumentationService*>(data);
  std::string s("0xbeefdead");
  I->store_value("test_key", new vecu8(s.begin(), s.end()));
}

// TODO(shmocz): ditch the old hook/cb test functions to use the common
// functions
std::map<std::string, command::Command::handler_t> get_commands_nn() {
  return {
      get_cmd<ra2yrproto::commands::StoreValue>([](auto* Q) {
        // NB: ensure correct radix
        auto& a = Q->command_data();
        auto v = new vecu8(a.value().begin(), a.value().end());
        auto [lk, s] = Q->I()->aq_storage();
        Q->I()->store_value(a.key(), v);
      }),
      get_cmd<ra2yrproto::commands::GetSystemState>([](auto* Q) {
        auto* state = Q->command_data().mutable_state();
        auto* srv = Q->I()->ws_server_.get();
        srv->service_->post([state, srv]() {
          for (const auto& [socket_id, c] : srv->ws_conns) {
            auto* conn = state->add_connections();
            conn->set_socket_id(socket_id);
            duration_t dur = c.timestamp.time_since_epoch();
            conn->set_timestamp(dur.count());
          }
        });
        auto [l, rq] = Q->I()->cmd_manager().aq_results_queue();
        for (const auto& [k, v] : *rq) {
          state->add_queues()->set_queue_id(k);
        }
        state->set_directory(process::getcwd());
      }),
      get_cmd<ra2yrproto::commands::GetValue>([](auto* Q) {
        // NB: ensure correct radix
        // FIXME: proper locking
        auto [lk, s] = Q->I()->aq_storage();
        auto& c = Q->command_data();
        c.set_value(yrclient::to_string(
            *reinterpret_cast<vecu8*>(Q->I()->get_value(c.key(), false))));
      }),
      get_cmd<ra2yrproto::commands::HookableCommand>([](auto* Q) {
        static TestProgram t;
        auto t_addr = t.get_code();
        t_addr(3, 3);

        // yrclient::HookableCommand::Result res;
        auto& res = Q->command_data();
        res.set_address_test_function(reinterpret_cast<u64>(t_addr));
        res.set_address_test_callback(reinterpret_cast<u64>(&test_cb));
        res.set_code_size(t.entry_size);
      }),
      get_cmd<ra2yrproto::commands::AddCallback>([](auto* Q) {
        auto& a = Q->command_data();
        Q->I()
            ->hooks()
            .at(reinterpret_cast<u8*>(a.hook_address()))
            .add_callback(
                reinterpret_cast<hook::Hook::hook_cb_t>(a.callback_address()),
                Q->I(), "", 0u);
      }),
      get_cmd<ra2yrproto::commands::CreateHooks>([](auto* Q) {
        // TODO(shmocz): put these to utility function and share code with
        // Hook code.
        auto P = process::get_current_process();
        std::vector<process::thread_id_t> ns(Q->I()->get_connection_threads());

        auto& a = Q->command_data();

        // suspend threads?
        if (!a.no_suspend_threads()) {
          ns.push_back(process::get_current_tid());
          P.suspend_threads(ns);
        }

        // create hooks
        for (auto& h : a.hooks()) {
          Q->I()->create_hook(h.name(), reinterpret_cast<u8*>(h.address()),
                              h.code_length());
        }
        if (!a.no_suspend_threads()) {
          P.resume_threads(ns);
        }
      })};
}

std::map<std::string, command::Command::handler_t>
yrclient::commands_builtin::get_commands() {
  return get_commands_nn();
}
