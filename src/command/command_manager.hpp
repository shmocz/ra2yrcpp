#pragma once

#include "async_queue.hpp"
#include "command.hpp"
#include "config.hpp"
#include "ring_buffer.hpp"
#include "types.h"
#include "utility/sync.hpp"

#include <cstdint>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace command {

namespace {
using namespace std::chrono_literals;
}

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
  CommandEntry(const std::string name, Command::handler_t handler);
  const std::string name_;
  Command::handler_t handler_;
};

/// Constructs new Command instances
class CommandFactory {
 public:
  CommandFactory();
  void add_entry(const std::string name, Command::handler_t handler);
  Command* make_command(const std::string name, void* args,
                        const std::uint64_t queue_id);
  Command* make_command(const std::string name,
                        std::unique_ptr<void, void (*)(void*)> args,
                        const std::uint64_t queue_id);

 private:
  std::map<std::string, Command::handler_t> handlers_;
  std::map<std::string, CommandEntry> entries_;
  std::uint64_t counter_{0u};
};

using result_queue_t =
    async_queue::AsyncQueue<std::shared_ptr<Command>,
                            ring_buffer::RingBuffer<std::shared_ptr<Command>>>;

using results_queue_t = std::map<uint64_t, std::shared_ptr<result_queue_t>>;

class CommandManager {
 public:
  explicit CommandManager(const duration_t results_acquire_timeout =
                              cfg::COMMAND_RESULTS_ACQUIRE_TIMEOUT);
  /// Shutdown and join worker
  ~CommandManager();

  /// Create new result queue.
  void create_queue(const uint64_t id, const std::size_t max_size = -1u);
  void destroy_queue(const uint64_t id);
  void invoke_user_command(std::shared_ptr<Command> cmd);
  /// Enqueue command and store result to given queue
  void enqueue_command(std::shared_ptr<Command> cmd);
  /// Enqueue execution of a built-in command
  void enqueue_builtin(const CommandType type, const int queue_id,
                       const BuiltinArgs args = {});
  /// Worker loop that consumes command from input queue and stores result to
  /// appropriate queue.
  void worker();
  /// Start the main worker thread.
  /// @exception std::runtime_error if a worker thread has already been created
  void start();
  /// Puts a shutdown Command to work queue
  void shutdown();
  ///
  CommandFactory& factory();
  util::acquire_t<results_queue_t*, std::timed_mutex> aq_results_queue();
  ///
  /// Remove and return all non-pending items from a given queue.
  ///
  /// @param id Queue id
  /// @param timeout How long to wait for items to appear if the queue is empty
  /// @param count Number of items to retrieve. If 0, retrieve all items.
  /// @throws std::out_of_range if queue with the given id wasn't found
  ///
  std::vector<std::shared_ptr<Command>> flush_results(
      const uint64_t id, const duration_t timeout = 0.0s,
      const std::size_t count = 0u);
  std::vector<std::shared_ptr<Command>>& pending_commands();

 private:
  std::atomic_bool active_{true};
  duration_t results_acquire_timeout_;
  std::unique_ptr<std::thread> worker_thread_;
  CommandFactory factory_;
  std::priority_queue<std::shared_ptr<Command>,
                      std::vector<std::shared_ptr<Command>>, QueueCompare>
      work_queue_;
  std::mutex pending_commands_mut_;
  std::vector<std::shared_ptr<Command>> pending_commands_;
  std::condition_variable work_queue_cv_;
  std::mutex work_queue_mut_;
  results_queue_t results_queue_;
  std::timed_mutex mut_results_;
};
}  // namespace command
