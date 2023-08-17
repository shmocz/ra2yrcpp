#include "command/command_manager.hpp"

#include "command/command.hpp"
#include "errors.hpp"
#include "logging.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

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

CommandManager::CommandManager(const duration_t results_acquire_timeout)
    : results_acquire_timeout_(results_acquire_timeout) {}

CommandManager::~CommandManager() {}

void CommandManager::start() {
  if (worker_thread_ != nullptr) {
    throw std::runtime_error("command manager worker thread already exists");
  }
  worker_thread_ = std::make_unique<std::thread>([this]() {
    try {
      this->worker();
    } catch (const std::exception& e) {
      eprintf("worker died {}", e.what());
    }
  });
}

void CommandManager::shutdown() {
  enqueue_builtin(CommandType::SHUTDOWN, 0);
  worker_thread_->join();
}

CommandFactory& CommandManager::factory() { return factory_; }

util::acquire_t<results_queue_t*, std::timed_mutex>
CommandManager::aq_results_queue() {
  try {
    return util::acquire(mut_results_, &results_queue_,
                         results_acquire_timeout_);
  } catch (const std::exception& e) {
    throw std::runtime_error(
        fmt::format("failed to acquire results queue within {}: {}",
                    results_acquire_timeout_, e.what()));
  }
}

void CommandManager::create_queue(const uint64_t id,
                                  const std::size_t max_size) {
  auto [l, q] = aq_results_queue();
  dprintf("queue_id={}", id);
  if (q->find(id) != q->end()) {
    throw std::runtime_error(fmt::format("existing queue_id={}", id));
  }
  q->try_emplace(
      id, std::make_shared<result_queue_t>(result_queue_t::queue_t(max_size)));
}

void CommandManager::destroy_queue(const uint64_t id) {
  auto [l, q] = aq_results_queue();
  dprintf("queue_id={}", id);
  q->erase(id);
}

void CommandManager::invoke_user_command(std::shared_ptr<Command> cmd) {
  cmd->result_code().store(ResultCode::NONE);
  try {
    cmd->run();
    cmd->result_code().store(ResultCode::OK);
  } catch (std::exception& e) {
    eprintf("fail {}", e.what());
    cmd->error_message()->assign(e.what());
    cmd->result_code().store(ResultCode::ERROR);
  }

  auto [l, rq] = aq_results_queue();
  if (cmd->pending()) {
    std::unique_lock<decltype(pending_commands_mut_)> lp(pending_commands_mut_);
    pending_commands_.push_back(cmd);
  }

  if (cmd->discard_result()) {
    return;
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
      case CommandType::USER: {
        k.unlock();
        invoke_user_command(cmd);
        break;
      }
      default:
        throw yrclient::general_error("Unknown command type");
    }
  }
  iprintf("exit worker");
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
    const uint64_t id, const duration_t timeout, const std::size_t count) {
  auto [l, rq] = aq_results_queue();
  if (rq->find(id) == rq->end()) {
    throw std::out_of_range(fmt::format("no such queue {}", id));
  }
  auto q = rq->at(id);
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
