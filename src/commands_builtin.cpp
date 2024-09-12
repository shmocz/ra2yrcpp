#include "commands_builtin.hpp"

#include "ra2yrproto/commands_builtin.pb.h"

#include "asio_utils.hpp"
#include "command/is_command.hpp"
#include "hook.hpp"
#include "instrumentation_service.hpp"
#include "process.hpp"
#include "types.h"
#include "util_string.hpp"

#include <xbyak/xbyak.h>

#include <cstddef>

#include <utility>
#include <vector>

using ra2yrcpp::command::get_cmd;

std::map<std::string, ra2yrcpp::cmd_t::handler_t>
ra2yrcpp::commands_builtin::get_commands() {
  return {
      get_cmd<ra2yrproto::commands::StoreValue>([](auto* Q) {
        // NB: ensure correct radix
        auto& a = Q->command_data();
        auto [lk, s] = Q->I()->aq_storage();
        Q->I()->template store_value<vecu8>(a.key(), a.value().begin(),
                                            a.value().end());
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
        c.set_value(ra2yrcpp::to_string(
            *reinterpret_cast<vecu8*>(Q->I()->get_value(c.key(), false))));
      }),
  };
}
