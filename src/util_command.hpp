#pragma once
#include "command/command.hpp"
#include "protocol/protocol.hpp"
#include "instrumentation_service.hpp"
namespace util_command {

template <typename T>
struct ISCommand {
  using result_t = decltype(std::declval<T>().result());

  explicit ISCommand(command::Command* c)
      : c(c), a((yrclient::ISArgs*)c->args()) {
    result_q_ = new yrclient::NewResult();
    a->M->UnpackTo(&result_);
  }

  auto& args() { return result_.args(); }
  auto& result() { return result_; }
  void set_result(result_t val) { result_.set_result(val); }
  void save_command_result() {
    result_q_->mutable_body()->PackFrom(result_);
    c->set_result(reinterpret_cast<void*>(result_q_));
  }
  auto* I() { return a->I; }
  auto* M() { return a->M; }

  command::Command* c;
  yrclient::ISArgs* a;
  T result_;
  yrclient::NewResult* result_q_;
  yrclient::ISArgs* res;
};
}  // namespace util_command
