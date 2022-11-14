#pragma once
#include "debug_helpers.h"

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
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
  using deleter_t = std::function<void(Command*)>;

  struct methods_t {
    handler_t handler;
    deleter_t deleter;
  };

  Command() = delete;
  // For factory commands
  Command(const std::string name, handler_t handler,
          deleter_t deleter = nullptr);
  // For command instantiation
  Command(const std::string name, methods_t methods, std::uint64_t queue_id,
          std::uint64_t task_id, void* args,
          CommandType cmd_type = CommandType::USER);
  // For built-in commands
  Command(const CommandType type, const uint64_t queue_id,
          BuiltinArgs* args = nullptr);
  ~Command();
  void run();
  // Pointer to result data.
  void* result();
  void* args();
  CommandType type() const;
  std::uint64_t queue_id() const;
  std::uint64_t task_id() const;
  ResultCode* result_code();
  // FIXME: use directly ref. in result(). this is just to not to break old code
  void set_result(void* p);
  std::string* error_message();
  bool builtin() const;

 private:
  methods_t methods_;
  CommandType type_;        // is this a built-in command or something else?
  std::uint64_t queue_id_;  // such as socket
  std::uint64_t task_id_;   // unique task id
  // if i use void* i need to pass ptr to protobuf msg in IService
  // unique name. used by manager to call appropriate function. get from
  // protobuf descriptor
  std::string name_;
  // how these are processed is completely up to handler function
  void* args_;
  void* result_;
  ResultCode result_code_{ResultCode::NONE};
  std::string error_message_;
};

}  // namespace command
