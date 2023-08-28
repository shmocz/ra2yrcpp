#pragma once
#include "command/command_manager.hpp"
#include "logging.hpp"

#include <google/protobuf/any.pb.h>

#include <functional>
#include <string>
#include <utility>

namespace yrclient {
class InstrumentationService;
}

namespace ra2yrcpp {

namespace command {
struct ISArg {
  void* instrumentation_service;
  google::protobuf::Any M;
};

using iservice_cmd = Command<ISArg>;

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
  explicit ISCommand(iservice_cmd* c) : c(c) {
    c->command_data()->M.UnpackTo(&command_data_);
  }

  ISCommand(const ISCommand&) = delete;
  ISCommand& operator=(const ISCommand&) = delete;

  ~ISCommand() { save_command_result(); }

  auto& command_data() { return command_data_; }

  void save_command_result() {
    // replace result, but only if pending is not set
    if (!c->pending()) {
      auto& p = c->command_data()->M;
      p.PackFrom(command_data_);
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

  auto* I() {
    return reinterpret_cast<yrclient::InstrumentationService*>(
        c->command_data()->instrumentation_service);
  }

  auto* M() { return c->command_data()->M; }

  iservice_cmd* c;
  T command_data_;
};

template <typename MessageT>
std::pair<std::string, iservice_cmd::handler_t> get_cmd(
    std::function<void(ISCommand<MessageT>*)> fn) {
  return {MessageT().GetTypeName(), [=](iservice_cmd* c) {
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
auto message_result(iservice_cmd* cmd) {
  MessageT m;
  cmd->command_data()->M.UnpackTo(&m);
  return m;
}

}  // namespace command
}  // namespace ra2yrcpp