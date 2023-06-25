#pragma once

#include "async_queue.hpp"
#include "config.hpp"
#include "logging.hpp"

#include <chrono>
#include <exception>
#include <functional>
#include <thread>
#include <utility>

namespace utility {

namespace {
using namespace std::chrono_literals;
};

struct auto_thread {
  std::thread _thread;

  explicit auto_thread(std::function<void(void)> fn);

  auto_thread(const auto_thread& o) = delete;
  auto_thread& operator=(const auto_thread& o) = delete;
  auto_thread(auto_thread&& o) = delete;
  auto_thread& operator=(auto_thread&& o) = delete;

  ~auto_thread();
};

// TODO: why not pass fn and arg in the same struct?
template <typename T>
struct worker_util {
  struct work_item {
    bool destroy{false};
    T item;
    std::function<void(T&)> consume_fn;
  };

  async_queue::AsyncQueue<work_item> work;
  std::function<void(T&)> consumer_fn;
  utility::auto_thread t;

  explicit worker_util(std::function<void(T&)> consumer_fn,
                       std::size_t queue_size = 0U)
      : work(queue_size),
        consumer_fn(std::move(consumer_fn)),
        t([&]() { this->worker(); }) {}

  worker_util(const worker_util& o) = delete;
  worker_util& operator=(const worker_util& o) = delete;
  worker_util(worker_util&& o) = delete;
  worker_util& operator=(worker_util&& o) = delete;

  // FIXME: use push()
  ~worker_util() { work.push(work_item{true, {}, nullptr}); }

  void push(T item, std::function<void(T&)> cfn = nullptr) {
    work.push(work_item{false, item, cfn});
  }

  void worker() {
    try {
      while (true) {
        auto V = work.pop(1, cfg::MAX_TIMEOUT);
        if (V.empty()) {
          break;
        }
        auto w = V.back();
        if (w.destroy) {
          break;
        }
        try {
          if (w.consume_fn == nullptr) {
            consumer_fn(w.item);
          } else {
            w.consume_fn(w.item);
          }
        } catch (const std::exception& e) {
          eprintf("consumer: {}", e.what());
        }
      }
    } catch (const std::exception& x) {
      eprintf("worker died");
      throw;
    }
  }
};

}  // namespace utility
