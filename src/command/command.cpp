#include "command/command.hpp"

using namespace command;

Command::Command(const std::string name, handler_t handler,
                 std::uint64_t queue_id, std::uint64_t task_id,
                 std::unique_ptr<void, void (*)(void*)> args,
                 CommandType cmd_type)
    : handler_(handler),
      type_(cmd_type),
      queue_id_(queue_id),
      task_id_(task_id),
      name_(name),
      args_(std::move(args)),
      result_(nullptr, [](auto d) { (void)d; }),
      pending_(false) {}

Command::~Command() {}

void* Command::result() { return result_.get(); }

void* Command::args() { return args_.get(); }

CommandType Command::type() const { return type_; }

void Command::run() { handler_(this); }

std::uint64_t Command::queue_id() const { return queue_id_; }

std::uint64_t Command::task_id() const { return task_id_; }

ResultCode* Command::result_code() { return &result_code_; }

void Command::set_result(std::unique_ptr<void, void (*)(void*)> p) {
  result_ = std::move(p);
}

std::string* Command::error_message() { return &error_message_; }

std::atomic_bool& Command::pending() { return pending_; }
