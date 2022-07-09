#include "command_manager.hpp"

using namespace command;

CommandEntry::CommandEntry(const std::string name, Command::handler_t handler,
                           Command::deleter_t deleter)
    : name_(name), handler_(handler), deleter_(deleter) {}

bool QueueCompare::operator()(cmd_entry_t left, cmd_entry_t right) {
  return left->type() >= right->type();
}

CommandFactory::CommandFactory() {}

void CommandFactory::add_entry(const std::string name,
                               Command::handler_t handler,
                               Command::deleter_t deleter) {
  DPRINTF("add cmd %s\n", name.c_str());
  entries_.try_emplace(name, name, handler, deleter);
}

Command* CommandFactory::make_command(const std::string name, void* args,
                                      const std::uint64_t queue_id) {
  auto& e = entries_.at(name);
  auto* C =
      new Command(name, {e.handler_, e.deleter_}, queue_id, counter_, args);
  ++counter_;
  return C;
}

#if 1
Command* CommandFactory::make_builtin(const CommandType type,
                                      const std::uint64_t queue_id) {
  auto* C = new Command(type, queue_id);
  return C;
}
#endif

CommandManager::CommandManager() {
  worker_thread_ = std::thread([this]() { this->worker(); });
}

CommandManager::~CommandManager() {
  enqueue_builtin(CommandType::SHUTDOWN, 0);
  DPRINTF("joining\n");
  worker_thread_.join();
  DPRINTF("joined\n");
}

CommandFactory& CommandManager::factory() { return factory_; }

results_queue_t& CommandManager::results_queue() { return results_queue_; }

void CommandManager::create_queue(const uint64_t id) {
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  DPRINTF("queue_id=%llu\n", id);
  results_queue_[id] = std::shared_ptr<result_queue_t>(new result_queue_t());
}

void CommandManager::destroy_queue(const uint64_t id) {
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  DPRINTF("queue_id=%llu\n", id);
  results_queue_.erase(id);
}

void CommandManager::invoke_user_command(std::shared_ptr<Command> cmd) {
  *cmd->result_code() = ResultCode::NONE;
  try {
    cmd->run();
    *cmd->result_code() = ResultCode::OK;
  } catch (std::exception& e) {
    DPRINTF("fail %s\n", e.what());
    *cmd->result_code() = ResultCode::ERROR;
  }
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  auto q = results_queue_.at(cmd->queue_id());
  DPRINTF("task_id=%llu\n", cmd->task_id());
  q->push(cmd);
}

void CommandManager::enqueue_builtin(const CommandType type,
                                     const int queue_id) {
  std::unique_lock<std::mutex> k(work_queue_mut_);
  // FIXME: use uptr
  auto cmd = factory().make_builtin(type, queue_id);
  // TODO: rlock
  work_queue_.push(std::shared_ptr<Command>(cmd));
  work_queue_cv_.notify_all();
}

void CommandManager::enqueue_command(std::shared_ptr<Command> cmd) {
  std::unique_lock<std::mutex> k(work_queue_mut_);
  // FIXME: use uptr
  work_queue_.push(cmd);
  work_queue_cv_.notify_all();
// work_queue_cv_.wait(k, [this] { return work_queue_.size() > 0u; });
#if 0
  std::unique_lock<std::mutex> k(work_queue_mut_);
  work_queue_.push(command);
  return command.task_id;
#endif
}

void CommandManager::worker() {
  DPRINTF("Spawn worker\n");
  while (active_) {
    // Wait for work
    std::unique_lock<std::mutex> k(work_queue_mut_);
    work_queue_cv_.wait(k, [this] { return work_queue_.size() > 0u; });
    auto cmd = work_queue_.top();
    work_queue_.pop();
    switch (cmd->type()) {
      case CommandType::CREATE_QUEUE:
        create_queue(cmd->queue_id());
        break;
      case CommandType::DESTROY_QUEUE:
        destroy_queue(cmd->queue_id());
        break;
      case CommandType::SHUTDOWN:
        active_ = false;
        break;
      case CommandType::USER:
        invoke_user_command(cmd);
        break;
      default:
        throw yrclient::general_error("Unknown command type");
    }
  }
  DPRINTF("exit worker\n");
}

// NB. race condition if trying to flush queue, which is being destroyed at the
// same time (e.g. polling a queue of disconnectin connection). Hence store
// queues as shared_ptrs.
std::vector<std::shared_ptr<Command>> CommandManager::flush_results(
    const uint64_t id, const std::chrono::milliseconds timeout,
    const std::size_t count) {
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  std::vector<cmd_entry_t> r;
  if (results_queue_.find(id) == results_queue_.end()) {
    throw std::out_of_range(std::string("no such queue ") + std::to_string(id));
  }
  auto q = results_queue_.at(id);
  l.unlock();
  auto res = q->pop(count, timeout);
  DPRINTF("res_size=%d\n", (int)res.size());
  return res;
}
