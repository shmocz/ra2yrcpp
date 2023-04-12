#pragma once
#include "logging.hpp"
#include "utility/time.hpp"

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace async_queue {

namespace {
using namespace std::chrono_literals;
}

struct AsyncData {
  std::mutex m;
  std::condition_variable cv;
};

class AsyncContainer {
 public:
  AsyncContainer() : a_(std::make_unique<AsyncData>()) {}

  AsyncContainer(AsyncContainer& o) : a_(o.a_) {}

  void notify_all() { a_.get()->cv.notify_all(); }

 protected:
  std::shared_ptr<AsyncData> a_;
};

// NB: pop method is undefined for empty queues - ensure that this is handled
// correctly elsewhere
template <typename T, typename QueueT = std::queue<T>>
class AsyncQueue : public AsyncContainer {
 public:
  using queue_t = QueueT;

  explicit AsyncQueue(std::size_t max_size = 0U)
      : max_size_(max_size), AsyncContainer() {}

  explicit AsyncQueue(QueueT q, std::size_t max_size = 0U)
      : AsyncContainer(), q_(q), max_size_(max_size) {}

  AsyncQueue(const AsyncQueue& o)
      : AsyncContainer(o), q_(o.q_), max_size_(o.max_size_) {}

  AsyncQueue(AsyncQueue&& o)
      : AsyncContainer(o), q_(o), max_size_(o.max_size_) {}

  AsyncQueue& operator=(const AsyncQueue& o) {
    a_ = o.a_;
    q_ = o.q_;
    max_size_ = o.max_size_;
    return *this;
  }

  ///
  /// Put an item to the queue, notifying everyone waiting for new items. If the
  /// queue is bounded, block until free space is available.
  ///
  void push(T t) {
    std::unique_lock<std::mutex> l(a_.get()->m);
    if (max_size_ > 0 && size() + 1 > max_size_) {
      a_.get()->cv.wait(l, [&] { return size() + 1 <= max_size_; });
    }
    q_.push(t);
#ifdef LOG_TRACE
    dprintf("notifying, a={},sz={}", reinterpret_cast<void*>(a_.get()), size());
#endif
    notify_all();
  }

  // Pop items from queue. If count < 1, pop all items. If timeout > 0, block
  // and wait up to that amount for results.
  std::vector<T> pop(const std::size_t count = 1,
                     const std::chrono::milliseconds timeout = 0ms,
                     std::function<bool(T&)> predicate = nullptr) {
    std::unique_lock<std::mutex> l(a_.get()->m);
#ifdef LOG_TRACE
    dprintf("locked={},asyncdata={},count={},timeout={}", l.owns_lock(),
            reinterpret_cast<void*>(a_.get()), count, timeout.count());
#endif
    std::vector<T> res;
    std::vector<T> pred_false;
    do {
      if (timeout > 0ms) {
        if (!a_.get()->cv.wait_for(l, timeout, [&] { return size() > 0; })) {
          return res;
        }
      } else if (empty()) {
        return res;
      }
      int num_pop = count < 1 ? size() : std::min(count, size());
      // TODO: use random access container to avoid popping and pushing back
      while (num_pop-- > 0) {
        auto p = q_.front();
        if (predicate != nullptr && !predicate(p)) {
          pred_false.push_back(p);
        } else {
          res.push_back(p);
        }
        q_.pop();
      }
      for (auto p : pred_false) {
        q_.push(p);
      }
    } while (res.size() < count);
    return res;
  }

  bool empty() const { return size() == 0; }

  std::size_t size() const { return q_.size(); }

 private:
  QueueT q_;
  std::size_t max_size_;
};
};  // namespace async_queue
