#include "command_manager.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "utility.h"
#include <mutex>

using namespace command_manager;

bool QueueCompare::operator()(Command left, Command right) {
  return left.type >= right.type;
}

CommandManager::CommandManager() {}

CommandManager::~CommandManager() {
  // Put stop signal to work queue
}

void CommandManager::add_command(std::string name, fn_command cmd) {
  std::lock_guard<std::mutex> k(commands_mutex_);
  if (commands_.find(name) != commands_.end()) {
    throw yrclient::general_error("Command already exists " + name);
  }
  commands_[name] = cmd;
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

void CommandManager::store_result(std::size_t task_id, CommandResult result) {
  std::lock_guard<std::mutex> lock(result_mutex_);
  auto& qu = result_queue_.at(task_id);
  qu.push(result);
}

void CommandManager::create_result_queue(const int id) {
  throw yrclient::not_implemented();
}
void CommandManager::destroy_result_queue(const int id) {
  throw yrclient::not_implemented();
}

void CommandManager::worker() {
  bool active = true;
  while (active) {
    DPRINTF("Spawn worker\n");
    std::unique_lock<std::mutex> k(work_queue_mut_);
    work_queue_cv_.wait(k, [this] { return work_queue_.size() > 0u; });
    auto cmd = work_queue_.top();
    work_queue_.pop();
    if (cmd.type == CommandType::CREATE_QUEUE) {
      create_result_queue(std::stoi(cmd.args));
    } else if (cmd.type == CommandType::DESTROY_QUEUE) {
      destroy_result_queue(std::stoi(cmd.args));
    } else if (cmd.type == CommandType::USER_DEFINED) {
      throw yrclient::not_implemented();
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
