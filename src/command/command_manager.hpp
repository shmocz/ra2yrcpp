#pragma once

#include "command.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "utility/sync.hpp"
#include "utility/time.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

namespace command {

using cmd_entry_t = std::shared_ptr<Command>;

class QueueCompare {
 public:
  bool operator()(cmd_entry_t left, cmd_entry_t right);
};

///
/// This holds command's init and cleanup functions. And is used as basis for
/// instantiating new commands.
///
class CommandEntry {
 public:
  CommandEntry(const std::string name, Command::handler_t handler,
               Command::deleter_t deleter = nullptr);
  const std::string name_;
  Command::handler_t handler_;
  Command::deleter_t deleter_;
};

/// Constructs new Command instances
class CommandFactory {
 public:
  CommandFactory();
  void add_entry(const std::string name, Command::handler_t handler,
                 Command::deleter_t deleter = nullptr);
  Command* make_command(const std::string name, void* args,
                        const std::uint64_t queue_id);
  Command* make_builtin(const CommandType type, const std::uint64_t queue_id);

 private:
  std::map<std::string, Command::handler_t> handlers_;
  std::map<std::string, CommandEntry> entries_;
  std::uint64_t counter_{0u};
};

template <typename T>
using guarded = std::tuple<std::unique_lock<std::mutex>, T*>;

using results_queue_t =
    std::map<uint64_t, std::queue<std::shared_ptr<Command>>>;

class CommandManager {
 public:
  CommandManager();
  /// Shutdown and join worker
  ~CommandManager();

  void create_queue(const uint64_t id);
  void destroy_queue(const uint64_t id);
  void invoke_user_command(std::shared_ptr<Command> cmd);
  /// Enqueue command and store result to given queue
  void enqueue_command(std::shared_ptr<Command> cmd);
  /// Enqueue execution of a built-in command
  void enqueue_builtin(const CommandType type, const int queue_id);
  /// Worker loop that consumes command from input queue and stores result to
  /// appropriate queue.
  void worker();
  /// Puts a shutdown Command to work queue
  void shutdown();
  ///
  CommandFactory& factory();
  guarded<results_queue_t> results_queue();
  std::vector<std::shared_ptr<Command>> flush_results(const uint64_t id);

 private:
  std::atomic_bool active_{true};
  std::thread worker_thread_;
  CommandFactory factory_;
  std::priority_queue<std::shared_ptr<Command>,
                      std::vector<std::shared_ptr<Command>>, QueueCompare>
      work_queue_;
  std::condition_variable work_queue_cv_;
  std::mutex work_queue_mut_;
  results_queue_t results_queue_;
  std::mutex mut_results_;
};
}  // namespace command
