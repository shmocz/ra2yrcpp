#pragma once
#include "debug_helpers.h"
#include "utility/time.hpp"

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <condition_variable>
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

  void notify_all() { a_.get()->cv.notify_all(); }

 protected:
  std::shared_ptr<AsyncData> a_;
};

// NB: pop method is undefined for empty queues - ensure that this is handled
// correctly elsewhere
template <typename T>
class AsyncQueue : public AsyncContainer {
 public:
  AsyncQueue() : AsyncContainer() {}
  AsyncQueue(AsyncQueue& o) {
    a_ = o.a_;
    q_ = o.q_;
  }
  AsyncQueue(AsyncQueue&& o) {
    a_ = o.a_;
    q_ = o.q_;
  }
  AsyncQueue& operator=(const AsyncQueue& o) {
    a_ = o.a_;
    q_ = o.q_;
    return *this;
  }
  // Put item to queue and notify
  void push(T t) {
    std::unique_lock<std::mutex> l(a_.get()->m);
    q_.push(t);
    DPRINTF("notifying, a=%p,sz=%u\n", a_.get(), size());
    notify_all();
  }
  // Pop items from queue. If count < 1, pop all items. If timeout > 0, block
  // and wait up to that amount for results.
  std::vector<T> pop(const std::size_t count = 1,
                     const std::chrono::milliseconds timeout = 0ms) {
    std::unique_lock<std::mutex> l(a_.get()->m);
    DPRINTF("locked=%d,asyncdata=%p,count=%u,timeout=%lld\n", l.owns_lock(),
            a_.get(), count, timeout.count());
    std::vector<T> res;
    do {
      if (timeout > 0ms) {
        if (!a_.get()->cv.wait_for(l, timeout, [&] { return size() > 0; })) {
          return res;
        }
      } else if (empty()) {
        return res;
      }
      int num_pop = count < 1 ? size() : std::min(count, size());
      while (num_pop-- > 0) {
        auto p = q_.front();
        res.push_back(p);
        q_.pop();
      }
    } while (res.size() < count);
    return res;
  }
  bool empty() const { return size() == 0; }
  std::size_t size() const { return q_.size(); }

 private:
  std::queue<T> q_;
};
};  // namespace async_queue
