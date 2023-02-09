#pragma once
#include "protocol/protocol.hpp"

#include "command/command.hpp"
#include "instrumentation_service.hpp"

#include <memory>
#include <string>
#include <utility>

namespace util_command {

//
// Wrapper which takes Protobuf Message and constructs a command function
// compatible with that message type, and provides access to
// InstrumentationService.
//
template <typename T>
struct ISCommand {
  using result_t = decltype(std::declval<T>().result());

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

  auto& args() { return command_data_.args(); }

  auto& command_data() { return command_data_; }

  void set_result(result_t val) { command_data_.set_result(val); }

  void save_command_result() {
    // replace result, but only if pending is not set
    if (!c->pending()) {
      auto* p = reinterpret_cast<ra2yrproto::CommandResult*>(result_q_.get());
      p->set_command_id(c->task_id());
      p->mutable_result()->PackFrom(command_data_);
      c->set_result(std::move(result_q_));
    }
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

}  // namespace util_command
