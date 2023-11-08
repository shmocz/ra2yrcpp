#pragma once
#include "command/command_manager.hpp"
#include "logging.hpp"

#include <google/protobuf/any.pb.h>

#include <functional>
#include <string>
#include <utility>

namespace ra2yrcpp {
class InstrumentationService;
}

namespace ra2yrcpp {

namespace gpb = google::protobuf;

namespace command {
struct ISArg {
  void* instrumentation_service;
  gpb::Any M;
};

using iservice_cmd = Command<ISArg>;

///
/// Wrapper which takes a command function compatible with a supplied
/// protobuf message type, and provides access to InstrumentationService.
///
/// NOTE: in async commands, don't access the internal data of the object after
/// exiting the command (e.g. executing a gameloop callback), because the object
/// is destroyed. To access the original args, make a copy and pass by value.
/// See mission_clicked() in commands_yr.cpp for example.
///
/// With async commands, the command data is automatically cleared after
/// command function execution, as it doesn't contain anything meaningful
/// regarding the eventual results.
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
    if (!c->pending().get()) {
      auto& p = c->command_data()->M;
      p.PackFrom(command_data_);
    } else {
      command_data_.Clear();
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
    return reinterpret_cast<ra2yrcpp::InstrumentationService*>(
        c->command_data()->instrumentation_service);
  }

  auto* M() { return &c->command_data()->M; }

  iservice_cmd* c;
  T command_data_;
};

template <typename MessageT>
std::pair<std::string, iservice_cmd::handler_t> get_cmd(
    std::function<void(ISCommand<MessageT>*)> fn, bool async = false) {
  return {MessageT().GetTypeName(), [=](iservice_cmd* c) {
            ISCommand<MessageT> Q(c);
            if (async) {
              Q.async();
            }
            try {
              dprintf("exec {} ", MessageT().GetTypeName());
              fn(&Q);
            } catch (...) {
              Q.M()->Clear();
              throw;
            }
          }};
}

template <typename MessageT>
std::pair<std::string, iservice_cmd::handler_t> get_async_cmd(
    std::function<void(ISCommand<MessageT>*)> fn) {
  return get_cmd(fn, true);
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
