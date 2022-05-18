#include "command_manager.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "utility.h"
#include <mutex>
#include <queue>
#include <utility>
#include <tuple>

using namespace command_manager;

bool QueueCompare::operator()(Command left, Command right) {
  return left.type >= right.type;
}

CommandManager::CommandManager() {
  worker_ = std::thread([this]() { this->worker(); });
}

CommandManager::~CommandManager() {
  // Put stop signal to work queue
  run_command(shutdown());

  worker_.join();
}

void CommandManager::add_command(std::string name, fn_command cmd) {
  std::lock_guard<std::mutex> k(commands_mutex_);
  if (commands_.find(name) != commands_.end()) {
    throw yrclient::general_error("Command already exists " + name);
  }
  commands_[name] = cmd;
  DPRINTF("Added command %s, ptr=%p\n", name.c_str());
}

uint64_t CommandManager::run_command(const int queue_id, std::string name,
                                     std::string args) {
  DPRINTF("queue_id=%d,name=%s,args=%s\n", queue_id, name.c_str(),
          args.c_str());
  auto tid =
      run_command(Command{CommandType::USER_DEFINED, queue_id, 0u, name, args});
  return tid;
}

uint64_t CommandManager::run_command(Command command) {
  std::unique_lock<std::mutex> lc(commands_mutex_);

  // User defined?
  if (command.type == CommandType::USER_DEFINED) {
    if (commands_.find(command.name) == commands_.end()) {
      throw yrclient::general_error("No such command: " + command.name);
    }
  }
  // Get next task id
  command.task_id = task_counter_++;

  std::unique_lock<std::mutex> k(work_queue_mut_);
  work_queue_.push(command);
  work_queue_cv_.notify_one();
  return command.task_id;
}

std::tuple<std::unique_lock<std::mutex>, result_queue_t*>
CommandManager::result_queue() {
  return std::make_tuple(std::unique_lock<std::mutex>(result_mutex_),
                         &result_queue_);
}

void CommandManager::store_result(std::size_t task_id, CommandResult&& result) {
  std::lock_guard<std::mutex> lock(result_mutex_);
  auto& qu = result_queue_.at(task_id);
  qu.push(std::move(result));
  DPRINTF("stored task %d, qsize=%d\n", task_id, qu.size());
}

void CommandManager::create_result_queue(const int id) {
  std::lock_guard<std::mutex> lock(result_mutex_);
  if (result_queue_.find(id) != result_queue_.end()) {
    throw yrclient::general_error("Result queue exists " + std::to_string(id));
  }
  result_queue_[id] = std::queue<CommandResult>();
}
void CommandManager::destroy_result_queue(const int id) {
  std::lock_guard<std::mutex> lock(result_mutex_);
  result_queue_.erase(id);
}

void CommandManager::worker() {
  bool active = true;
  DPRINTF("Spawn worker\n");
  while (active) {
    std::unique_lock<std::mutex> k(work_queue_mut_);
    work_queue_cv_.wait(k, [this] { return work_queue_.size() > 0u; });
    auto cmd = work_queue_.top();
    work_queue_.pop();
    if (cmd.type == CommandType::CREATE_QUEUE) {
      create_result_queue(cmd.queue_id);
    } else if (cmd.type == CommandType::DESTROY_QUEUE) {
      destroy_result_queue(cmd.queue_id);
    } else if (cmd.type == CommandType::USER_DEFINED) {
      // TODO: try-catch for failures
      std::unique_ptr<vecu8> result = nullptr;
      auto ecode = CommandResultCode::COMMAND_OK;
      try {
        result = commands_[cmd.name](&cmd.args);
      } catch (const std::exception& e) {
        ecode = CommandResultCode::COMMAND_ERROR;
        const std::string r(e.what());
        result = std::make_unique<vecu8>(r.begin(), r.end());
      }
      store_result(cmd.queue_id, CommandResult{std::move(result), ecode});
    } else if (cmd.type == CommandType::SHUTDOWN) {
      active = false;
    }
  }
}

Command CommandManager::create_queue(const int id) {
  return Command{CommandType::CREATE_QUEUE, id, 0u, "", ""};
}
Command CommandManager::destroy_queue(const int id) {
  return Command{CommandType::DESTROY_QUEUE, id, 0u, "", ""};
}

Command CommandManager::shutdown() {
  return Command{CommandType::SHUTDOWN, 0, 0u, "", ""};
}
