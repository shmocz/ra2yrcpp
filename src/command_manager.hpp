#pragma once
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <memory>
#include <tuple>
#include <thread>
#include <queue>
#include <string>
#include <vector>
#include <cstdint>
#include "types.h"

namespace command_manager {

enum CommandResultCode : int { COMMAND_OK = 0, COMMAND_ERROR = 1 };

using CommandResultData = std::unique_ptr<vecu8>;

struct CommandResult {
  CommandResultData data;
  CommandResultCode code;
};

using result_queue_t = std::map<size_t, std::queue<CommandResult>>;

enum CommandType : int {
  CREATE_QUEUE = 0,
  DESTROY_QUEUE,
  SHUTDOWN,
  USER_DEFINED
};

struct Command {
  CommandType type;
  int queue_id;
  std::uint64_t task_id;
  std::string name;
  std::string args;
};

struct ResultQueue {
  explicit ResultQueue(std::mutex mutex);

 private:
  std::mutex m_;
  std::queue<CommandResult> queue_;
};

class QueueCompare {
 public:
  bool operator()(Command left, Command right);
};

class CommandManager {
 public:
  using fn_command = std::function<std::unique_ptr<vecu8>(void*)>;
  CommandManager();
  ~CommandManager();
  void create_result_queue(const int id);
  void destroy_result_queue(const int id);
  void worker();

  void add_command(std::string name, fn_command cmd);
  /// Schedule command for execution. Store the result in matching queue, or
  /// discard it if the queue isn't available.
  uint64_t run_command(const int queue_id, std::string name, std::string args);
  uint64_t run_command(Command command);
  void store_result(std::size_t task_id, CommandResult&& result);

  /// Generate built-in queue creation command
  static Command create_queue(const int id);
  /// Generate built-in queue destroy command
  static Command destroy_queue(const int id);
  /// Generate built-in shutdown command
  static Command shutdown();
  /// Acquire result queue
  std::tuple<std::unique_lock<std::mutex>, result_queue_t*> result_queue();

 private:
  std::priority_queue<Command, std::vector<Command>, QueueCompare> work_queue_;
  std::mutex work_queue_mut_;
  std::condition_variable work_queue_cv_;
  result_queue_t result_queue_;
  std::mutex result_mutex_;
  std::atomic_uint64_t task_counter_{0u};
  // TODO: thread safety
  std::mutex commands_mutex_;
  std::map<std::string, fn_command> commands_;
  std::thread worker_;
};
}  // namespace command_manager
