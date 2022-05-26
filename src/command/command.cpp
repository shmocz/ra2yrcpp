#include "command.hpp"
#include <stdexcept>
#include "debug_helpers.h"

using namespace command;

Command::Command(const std::string name, handler_t handler, deleter_t deleter)
    : methods_({.handler = handler, .deleter = deleter}),
      type_(CommandType::USER),
      queue_id_(0),
      task_id_(0),
      name_(name),
      args_(nullptr),
      result_(nullptr) {}

Command::Command(const std::string name, methods_t methods,
                 std::uint64_t queue_id, std::uint64_t task_id, void* args)
    : methods_({.handler = methods.handler, .deleter = methods.deleter}),
      type_(CommandType::USER),
      queue_id_(queue_id),
      task_id_(task_id),
      name_(name),
      args_(args),
      result_(nullptr) {}

Command::Command(const CommandType type, const uint64_t queue_id)
    : type_(type), queue_id_(queue_id) {
  if (!(type_ == CommandType::DESTROY_QUEUE ||
        type_ == CommandType::CREATE_QUEUE || type_ == CommandType::SHUTDOWN)) {
    throw std::invalid_argument("Invalid command type");
  }
}

Command::~Command() {
  if (methods_.deleter) {
    methods_.deleter(this);
  }
}

void* Command::result() { return result_; }
void* Command::args() { return args_; }
CommandType Command::type() const { return type_; }

void Command::run() { methods_.handler(this); }

std::uint64_t Command::queue_id() const { return queue_id_; }
std::uint64_t Command::task_id() const { return task_id_; }
const std::string& Command::name() const { return name_; }
ResultCode* Command::result_code() { return &result_code_; }

Command::methods_t& Command::methods() { return methods_; }
void Command::set_result(void *p) { result_ = p; }