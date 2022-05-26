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

guarded<results_queue_t> CommandManager::results_queue() {
  return util::acquire(mut_results_, &results_queue_);
}

void CommandManager::create_queue(const uint64_t id) {
  auto [lk_rq, rq] = results_queue();
  // DPRINTF("id=%d, rq=%p\n", id, rq);
  (*rq)[id] = std::queue<std::shared_ptr<Command>>();
}

void CommandManager::destroy_queue(const uint64_t id) {
  auto [lk_rq, rq] = results_queue();
  // DPRINTF("id=%d,rq=%p\n", id, rq);
  rq->erase(id);
}

void CommandManager::invoke_user_command(std::shared_ptr<Command> cmd) {
  *cmd->result_code() = ResultCode::NONE;
  try {
    DPRINTF("run\n");
    cmd->run();
    *cmd->result_code() = ResultCode::OK;
  } catch (std::exception& e) {
    DPRINTF("fail %s\n", e.what());
    *cmd->result_code() = ResultCode::ERROR;
  }
  auto [lk_rq, rq] = results_queue();
  auto& q = rq->at(cmd->queue_id());
  q.push(cmd);
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
    DPRINTF("cmd: %s\n", cmd->name().c_str());
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

std::vector<std::shared_ptr<Command>> CommandManager::flush_results(
    const uint64_t id) {
  auto [lk_rq, rq] = results_queue();
  std::vector<cmd_entry_t> r;
  auto& q = rq->at(id);
  while (!q.empty()) {
    auto p = q.front();
    q.pop();
    r.push_back(p);
  }
  return r;
}
