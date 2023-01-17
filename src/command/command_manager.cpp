#include "command/command_manager.hpp"

using namespace command;

CommandEntry::CommandEntry(const std::string name, Command::handler_t handler)
    : name_(name), handler_(handler) {}

bool QueueCompare::operator()(cmd_entry_t left, cmd_entry_t right) {
  return left->type() >= right->type();
}

CommandFactory::CommandFactory() {}

void CommandFactory::add_entry(const std::string name,
                               Command::handler_t handler) {
  dprintf("add cmd {}", name.c_str());
  entries_.try_emplace(name, name, handler);
}

Command* CommandFactory::make_command(const std::string name, void* args,
                                      const std::uint64_t queue_id) {
  return make_command(
      name,
      std::unique_ptr<void, void (*)(void*)>(args, [](auto d) { (void)d; }),
      queue_id);
}

Command* CommandFactory::make_command(
    const std::string name, std::unique_ptr<void, void (*)(void*)> args,
    const std::uint64_t queue_id) {
  auto& e = entries_.at(name);
  auto* C = new Command(name, e.handler_, queue_id, counter_, std::move(args));
  ++counter_;
  return C;
}

CommandManager::CommandManager()
    : timeout_(0ms), worker_thread_([this]() {
        try {
          this->worker();
        } catch (const std::exception& e) {
          eprintf("worker died {}", e.what());
        }
      }) {}

CommandManager::~CommandManager() {
  enqueue_builtin(CommandType::SHUTDOWN, 0);
  dprintf("joining");
  worker_thread_.join();
  dprintf("joined");
}

CommandFactory& CommandManager::factory() { return factory_; }

results_queue_t& CommandManager::results_queue() { return results_queue_; }

void CommandManager::create_queue(const uint64_t id,
                                  const std::size_t max_size) {
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  dprintf("queue_id={}", id);
  if (results_queue_.find(id) != results_queue_.end()) {
    throw std::runtime_error(fmt::format("existsing queue_id={}", id));
  }
  results_queue_[id] =
      std::make_shared<result_queue_t>(result_queue_t::queue_t(max_size));
}

void CommandManager::destroy_queue(const uint64_t id) {
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  dprintf("queue_id={}", id);
  results_queue_.erase(id);
}

void CommandManager::invoke_user_command(std::shared_ptr<Command> cmd) {
  *cmd->result_code() = ResultCode::NONE;
  try {
    cmd->run();
    *cmd->result_code() = ResultCode::OK;
  } catch (std::exception& e) {
    dprintf("fail {}", e.what());
    cmd->error_message()->assign(e.what());
    *cmd->result_code() = ResultCode::ERROR;
  }
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  if (cmd->pending()) {
    std::unique_lock<decltype(pending_commands_mut_)> lp(pending_commands_mut_);
    pending_commands_.push_back(cmd);
  }
  auto q = results_queue_.at(cmd->queue_id());
  q->push(cmd);
}

void CommandManager::enqueue_builtin(const CommandType type, const int queue_id,
                                     BuiltinArgs args) {
  std::unique_lock<std::mutex> k(work_queue_mut_);
  // FIXME: use uptr
  // TODO: rlock to avoid copypaste code
  work_queue_.push(std::shared_ptr<Command>(
      new Command("", nullptr, queue_id, 0u,
                  std::unique_ptr<void, void (*)(void*)>(
                      new BuiltinArgs(args),
                      [](auto d) { delete reinterpret_cast<BuiltinArgs*>(d); }),
                  type)));
  work_queue_cv_.notify_all();
}

void CommandManager::enqueue_command(std::shared_ptr<Command> cmd) {
  std::unique_lock<std::mutex> k(work_queue_mut_);
  // FIXME: use uptr
  work_queue_.push(cmd);
  work_queue_cv_.notify_all();
}

void CommandManager::worker() {
  dprintf("Spawn worker");
  while (active_) {
    // Wait for work
    std::unique_lock<std::mutex> k(work_queue_mut_);
    work_queue_cv_.wait(k, [this] { return work_queue_.size() > 0u; });
    auto cmd = work_queue_.top();
    work_queue_.pop();
    switch (cmd->type()) {
      case CommandType::CREATE_QUEUE:
        create_queue(cmd->queue_id(),
                     reinterpret_cast<BuiltinArgs*>(cmd->args())->queue_size);
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
  dprintf("exit worker");
}

// NB. race condition if trying to flush queue, which is being destroyed at the
// same time (e.g. polling a queue of disconnectin connection). Hence store
// queues as shared_ptrs.
//
// Some commands (like PlaceQuery) are async - their results may not be
// instantly available. To handle this, add an optional "pending" flag to the
// command, and only flush the result after it has been cleared. Also ensure
// thread safety of command's internal state manipulation.
std::vector<std::shared_ptr<Command>> CommandManager::flush_results(
    const uint64_t id, const std::chrono::milliseconds timeout,
    const std::size_t count) {
  std::unique_lock<decltype(mut_results_)> l(mut_results_, timeout_);
  if (results_queue_.find(id) == results_queue_.end()) {
    throw std::out_of_range(std::string("no such queue ") + std::to_string(id));
  }
  auto q = results_queue_.at(id);
  l.unlock();
  // pop all non pending results
  auto res =
      q->pop(count, timeout, [](auto& c) { return !c->pending().load(); });
  std::unique_lock<decltype(pending_commands_mut_)> lk(pending_commands_mut_);
  auto& p = pending_commands();
  p.erase(std::remove_if(p.begin(), p.end(),
                         [&res](auto& c) {
                           return std::find_if(
                                      res.begin(), res.end(), [&c](auto& d) {
                                        return d->task_id() == c->task_id();
                                      }) != res.end();
                         }),
          p.end());
  return res;
}

std::vector<std::shared_ptr<Command>>& CommandManager::pending_commands() {
  return pending_commands_;
}
