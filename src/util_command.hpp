#pragma once

#include "ra2yrproto/core.pb.h"

#include "command/command.hpp"
#include "instrumentation_service.hpp"

#include <memory>
#include <string>
#include <utility>

namespace util_command {

///
/// Wrapper which takes constructs a command function compatible with a supplied
/// protobuf message type, and provides access to InstrumentationService.
///
/// NOTE: in async commands, don't access the internal data of the object after
/// exiting the command (e.g. executing a gameloop callback), because the object
/// is destroyed. To access the original args, make a copy and pass by value.
/// See mission_clicked() in commands_yr.cpp for example.
///
/// TODO(shmocz): make a mechanism to wrap async commands so that pending flag
/// is cleared automatically after execution or after exception.
///
template <typename T>
struct ISCommand {
  explicit ISCommand(command::Command* c)
      : c(c),
        a_((yrclient::ISArgs*)c->args()),
        result_q_(std::unique_ptr<void, void (*)(void*)>(
            new ra2yrproto::CommandResult(), [](auto p) {
              delete reinterpret_cast<ra2yrproto::CommandResult*>(p);
            })) {
    a_->M.UnpackTo(&command_data_);
  }

  ISCommand(const ISCommand&) = delete;
  ISCommand& operator=(const ISCommand&) = delete;

  ~ISCommand() { save_command_result(); }

  auto& command_data() { return command_data_; }

  void save_command_result() {
    // replace result, but only if pending is not set
    if (!c->pending()) {
      auto* p = reinterpret_cast<ra2yrproto::CommandResult*>(result_q_.get());
      p->set_command_id(c->task_id());
      p->mutable_result()->PackFrom(command_data_);
      c->set_result(std::move(result_q_));
    }
  }

  ///
  /// Mark this command as async. Allocate the command result and set the
  /// pending flag.
  ///
  void async() {
    save_command_result();
    c->pending().store(true);
  }

  auto* I() { return a_->I; }

  auto* M() { return a_->M; }

  command::Command* c;
  yrclient::ISArgs* a_;
  T command_data_;
  std::unique_ptr<void, void (*)(void*)> result_q_;
};

template <typename MessageT>
std::pair<std::string, command::Command::handler_t> get_cmd(
    std::function<void(ISCommand<MessageT>*)> fn) {
  return {MessageT().GetTypeName(), [=](command::Command* c) {
            ISCommand<MessageT> Q(c);
            dprintf("exec {} ", MessageT().GetTypeName());
            fn(&Q);
          }};
}

///
/// Unpacks the message stored in cmd->result() to appropriate message type, and
/// returns pointer to the result and instance of the message.
///
template <typename MessageT>
auto message_result(command::Command* cmd) {
  MessageT m;
  auto* p = reinterpret_cast<ra2yrproto::CommandResult*>(cmd->result());
  p->mutable_result()->UnpackTo(&m);
  return std::make_tuple(p, m);
}

}  // namespace util_command
