#pragma once

#include "async_queue.hpp"
#include "config.hpp"
#include "logging.hpp"
#include "ring_buffer.hpp"
#include "types.h"
#include "utility/sync.hpp"

#include <fmt/chrono.h>

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace ra2yrcpp {

namespace command {

namespace {
using namespace std::chrono_literals;
}

enum class ResultCode { NONE = 0, OK, ERROR };

enum class CommandType { DESTROY_QUEUE = 1, CREATE_QUEUE, SHUTDOWN, USER };

template <typename T>
class Command {
 public:
  using data_t = T;
  using handler_t = std::function<void(Command<T>*)>;

  struct BaseData {
    std::string name;
    u64 queue_id;
    u64 task_id;
    u64 queue_size;  // built-int arg
    CommandType type;
    BaseData() = delete;
  };

  Command() = delete;

  Command(BaseData B, handler_t handler, T&& data)
      : base_data_(B),
        handler_(handler),
        command_data_(data),
        result_code_(ResultCode::NONE),
        pending_(false),
        discard_result_(false) {}

  // TODO(shmocz): could set error/result just here
  void run() { handler_(this); }

  // Pointer to args/result data. (protobuf.Any for protobuf based commands)
  T* command_data() { return &command_data_; }

  CommandType type() const { return base_data_.type; }

  u64 queue_id() const { return base_data_.queue_id; }

  u64 task_id() const { return base_data_.task_id; }

  std::size_t queue_size() const { return base_data_.queue_size; }

  util::AtomicVariable<ResultCode>& result_code() { return result_code_; }

  std::atomic_bool& pending() { return pending_; }

  std::atomic_bool& discard_result() { return discard_result_; }

  handler_t& handler() { return handler_; }

  std::string& error_message() { return error_message_; }

  void set_error(const char* msg) {
    error_message_.assign(msg);
    result_code_.store(ResultCode::ERROR);
    // TODO(shmocz): clear command data
  }

 private:
  BaseData base_data_;
  handler_t handler_;
  T command_data_;
  util::AtomicVariable<ResultCode> result_code_;
  std::atomic_bool pending_;
  std::atomic_bool discard_result_;
  std::string error_message_;
};

/// Executes command handlers in single thread.
/// Commands are std::function objects that accept T* as their parameter.
template <typename T>
class CommandManager {
 public:
  using command_t = Command<T>;
  using command_ptr_t = std::shared_ptr<command_t>;
  using handler_t = typename Command<T>::handler_t;
  // TODO: use worker_util
  using result_q_t =
      async_queue::AsyncQueue<command_ptr_t,
                              ring_buffer::RingBuffer<command_ptr_t>>;
  using results_q_t = std::map<u64, std::shared_ptr<result_q_t>>;

  class QueueCompare {
   public:
    bool operator()(command_ptr_t left, command_ptr_t right) {
      return left->type() >= right->type();
    }
  };

  explicit CommandManager(const duration_t results_acquire_timeout =
                              cfg::COMMAND_RESULTS_ACQUIRE_TIMEOUT)
      : results_acquire_timeout_(results_acquire_timeout),
        command_counter_(0U) {}

  void create_queue(const u64 id, const std::size_t max_size = -1u) {
    auto [l, q] = aq_results_queue();
    if (q->find(id) != q->end()) {
      eprintf("existing queue_id={}", id);
      return;
    }
    // TODO(shmocz) avoid referencing queue_t
    q->try_emplace(id, std::make_shared<result_q_t>(
                           typename result_q_t::queue_t(max_size)));
  }

  void destroy_queue(const u64 id) {
    auto [l, q] = aq_results_queue();
    q->erase(id);
  }

  auto aq_results_queue() {
    try {
      return util::acquire(&results_queue_, &mut_results_,
                           results_acquire_timeout_);
    } catch (const std::exception& e) {
      throw std::runtime_error(
          fmt::format("failed to acquire results queue within {}: {}",
                      results_acquire_timeout_, e.what()));
    }
  }

  void add_command(const std::string name, handler_t handler) {
    handlers_[name] = handler;
  }

  command_ptr_t make_command(const std::string name, T&& data,
                             const u64 queue_id) {
    std::unique_lock<std::mutex> l(command_counter_mut_);
    typename command_t::BaseData B = {name, queue_id, ++command_counter_, 0U,
                                      CommandType::USER};
    handler_t handler = nullptr;

    auto C =
        std::make_shared<command_t>(B, handlers_.at(name), std::move(data));
    return C;
  }

  /// Create a built-in command (type set to anything else than USER)
  ///
  /// @param queue_id Command's queue id
  /// @param queue_size command queue size for CREATE_QUEUE command
  /// @param t the command type
  /// @return the same shared_ptr to Command object
  command_ptr_t make_builtin_command(const u64 queue_id, const u64 queue_size,
                                     CommandType t) {
    typename command_t::BaseData B{"", queue_id, 0U, queue_size, t};
    auto C = std::make_shared<command_t>(B, nullptr, T());
    std::unique_lock<std::mutex> l(command_counter_mut_);
    command_counter_++;
    return C;
  }

  /// Push command message into work queue to be executed
  ///
  /// @param c shared_ptr to the Command object
  /// @return the same shared_ptr to Command object
  command_ptr_t enqueue_command(command_ptr_t c) {
    std::unique_lock<std::mutex> k(work_queue_mut_);
    // TODO(shmocz): use uptr
    work_queue_.push(c);
    work_queue_cv_.notify_all();
    return c;
  }

  /// Built-in command functions

  /// Synchronously Execute CREATE_QUEUE
  /// @param queue_id
  /// @param queue_size
  /// @return shared_ptr to the Command object
  command_ptr_t execute_create_queue(const u64 queue_id,
                                     const std::size_t queue_size = -1) {
    auto cmd = enqueue_command(
        make_builtin_command(queue_id, queue_size, CommandType::CREATE_QUEUE));
    cmd->result_code().wait_pred(
        [](ResultCode v) { return v != ResultCode::NONE; });
    return cmd;
  }

  /// Synchronously execute DESTROY_QUEUE command
  /// @param queue_id
  /// @return shared_ptr to the Command object
  command_ptr_t execute_destroy_queue(const u64 queue_id) {
    auto cmd = enqueue_command(
        make_builtin_command(queue_id, 0U, CommandType::DESTROY_QUEUE));
    cmd->result_code().wait_pred(
        [](ResultCode v) { return v != ResultCode::NONE; });
    return cmd;
  }

  /// Executes a Command of type USER, and puts result to appropriate result
  /// queue.
  ///
  /// On success, the Command's result code is set to OK.
  /// If the Command throws an exception, it's result code is set to ERROR.
  /// If the Command result is configured to be discarded, then it will not be
  /// stored into the corresponding result queue.
  ///
  /// @param cmd the Command object to be executed
  void invoke_user_command(command_ptr_t cmd) {
    cmd->result_code().store(ResultCode::NONE);
    try {
      cmd->run();
      cmd->result_code().store(ResultCode::OK);
    } catch (const std::exception& e) {
      eprintf("fail {}", e.what());
      cmd->set_error(e.what());
    }

    auto [l, rq] = aq_results_queue();
    if (cmd->pending()) {
      std::unique_lock<decltype(pending_commands_mut_)> lp(
          pending_commands_mut_);
      pending_commands_.push_back(cmd->task_id());
    }

    if (cmd->discard_result()) {
      return;
    }

    try {
      auto q = results_queue_.at(cmd->queue_id());
      q->push(cmd);
    } catch (const std::out_of_range& e) {
      eprintf("queue {} not found, result discarded", cmd->queue_id());
    }
  }

  std::vector<u64>& pending_commands() { return pending_commands_; }

  std::vector<command_ptr_t> flush_results(const u64 id,
                                           const duration_t timeout = 0.0s,
                                           const std::size_t count = 0U) {
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
                           [&res](u64 task_id) {
                             return std::find_if(res.begin(), res.end(),
                                                 [&task_id](auto& d) {
                                                   return d->task_id() ==
                                                          task_id;
                                                 }) != res.end();
                           }),
            p.end());
    return res;
  }

  void worker() {
    dprintf("Spawn worker");

    while (active_) {
      // Wait for work
      std::unique_lock<std::mutex> k(work_queue_mut_);
      work_queue_cv_.wait(k, [this] { return work_queue_.size() > 0u; });
      command_ptr_t cmd = work_queue_.top();
      work_queue_.pop();
      switch (cmd->type()) {
        case CommandType::CREATE_QUEUE: {
          create_queue(cmd->queue_id(), cmd->queue_size());
          cmd->result_code().store(ResultCode::OK);
        } break;
        case CommandType::DESTROY_QUEUE: {
          destroy_queue(cmd->queue_id());
          cmd->result_code().store(ResultCode::OK);
        } break;
        case CommandType::SHUTDOWN:
          active_ = false;
          break;
        case CommandType::USER: {
          k.unlock();
          invoke_user_command(cmd);
        } break;
        default:
          throw std::runtime_error("Unknown command type");
      }
    }
    iprintf("exit worker");
  }

  void start() {
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

  void shutdown() {
    enqueue_command(make_builtin_command(0U, 0U, CommandType::SHUTDOWN));
    worker_thread_->join();
  }

 private:
  std::atomic_bool active_{true};
  duration_t results_acquire_timeout_;
  std::size_t command_counter_;
  std::mutex command_counter_mut_;
  std::unique_ptr<std::thread> worker_thread_;
  std::priority_queue<command_ptr_t, std::vector<command_ptr_t>, QueueCompare>
      work_queue_;
  std::mutex pending_commands_mut_;
  std::vector<u64> pending_commands_;
  std::map<std::string, handler_t> handlers_;
  std::condition_variable work_queue_cv_;
  std::mutex work_queue_mut_;
  results_q_t results_queue_;
  std::timed_mutex mut_results_;
};
}  // namespace command

}  // namespace ra2yrcpp
