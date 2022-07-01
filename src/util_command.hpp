#pragma once
#include "protocol/protocol.hpp"

#include "command/command.hpp"
#include "instrumentation_service.hpp"
namespace util_command {

template <typename T>
struct ISCommand {
  using result_t = decltype(std::declval<T>().result());

  explicit ISCommand(command::Command* c)
      : c(c), a((yrclient::ISArgs*)c->args()) {
    result_q_ = new yrclient::CommandResult();
    a->M->UnpackTo(&command_data_);
  }

  ~ISCommand() { save_command_result(); }

  auto& args() { return command_data_.args(); }
  auto& command_data() { return command_data_; }
  void set_result(result_t val) { command_data_.set_result(val); }
  void save_command_result() {
    result_q_->set_command_id(c->task_id());
    result_q_->mutable_result()->PackFrom(command_data_);
    c->set_result(reinterpret_cast<void*>(result_q_));
  }
  auto* I() { return a->I; }
  auto* M() { return a->M; }

  command::Command* c;
  yrclient::ISArgs* a;
  T command_data_;
  yrclient::CommandResult* result_q_;
  yrclient::ISArgs* res;
};
}  // namespace util_command
