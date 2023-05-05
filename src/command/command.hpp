#pragma once
#include "utility/sync.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace command {

enum class ResultCode { NONE = 0, OK, ERROR };

enum class CommandType { DESTROY_QUEUE = 1, CREATE_QUEUE, SHUTDOWN, USER };

struct BuiltinArgs {
  std::size_t queue_size;
};

class Command {
 public:
  using handler_t = std::function<void(Command*)>;

  Command() = delete;
  // For command instantiation
  Command(const std::string name, handler_t handler, std::uint64_t queue_id,
          std::uint64_t task_id, std::unique_ptr<void, void (*)(void*)> args,
          CommandType cmd_type = CommandType::USER,
          bool discard_result = false);
  ~Command();
  void run();
  // Pointer to result data.
  void* result();
  void* args();
  CommandType type() const;
  std::uint64_t queue_id() const;
  std::uint64_t task_id() const;
  util::AtomicVariable<ResultCode>& result_code();
  void set_result(std::unique_ptr<void, void (*)(void*)> p);
  std::string* error_message();
  std::atomic_bool& pending();
  std::atomic_bool& discard_result();

 private:
  handler_t handler_;
  CommandType type_;        // is this a built-in command or something else?
  std::uint64_t queue_id_;  // such as socket
  std::uint64_t task_id_;   // unique task id
  // if i use void* i need to pass ptr to protobuf msg in IService
  // unique name. used by manager to call appropriate function. get from
  // protobuf descriptor
  std::string name_;
  // how these are processed is completely up to handler function
  std::unique_ptr<void, void (*)(void*)> args_;
  std::unique_ptr<void, void (*)(void*)> result_;
  util::AtomicVariable<ResultCode> result_code_;
  std::string error_message_;
  std::atomic_bool pending_;
  std::atomic_bool discard_result_;
};

}  // namespace command
