#pragma once

#include "async_queue.hpp"
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
  };

  async_queue::AsyncQueue<work_item> work;
  std::function<void(T&)> consumer_fn;
  utility::auto_thread t;

  explicit worker_util(std::function<void(T&)> consumer_fn)
      : consumer_fn(std::move(consumer_fn)), t([&]() { this->worker(); }) {}

  worker_util(const worker_util& o) = delete;
  worker_util& operator=(const worker_util& o) = delete;
  worker_util(worker_util&& o) = delete;
  worker_util& operator=(worker_util&& o) = delete;

  ~worker_util() { work.push(work_item{true, {}}); }

  void push(T item) { work.push(work_item{false, item}); }

  void worker() {
    try {
      while (true) {
        auto V = work.pop(1, 1000ms * (3600));
        auto w = V.back();
        if (w.destroy) {
          break;
        }
        try {
          consumer_fn(w.item);
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
