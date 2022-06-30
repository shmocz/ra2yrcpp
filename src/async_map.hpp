#pragma once
#include "async_queue.hpp"
#include "errors.hpp"
#include "utility/sync.hpp"
#include <condition_variable>
#include <map>
#include <mutex>
#include <chrono>
#include <ratio>

namespace async_map {

namespace {
using namespace std::chrono_literals;
}

template <typename PredT, typename LockT = std::mutex>
bool wait_until(std::unique_lock<LockT>* lock, std::condition_variable* cv,
                PredT pred, const std::chrono::milliseconds timeout = 0ms) {
  return (cv->wait_for(*lock, timeout, pred));
}

template <typename T, typename KeyT = std::uint64_t>
class AsyncMap : public async_queue::AsyncContainer {
 public:
  AsyncMap() {}
  void put(const KeyT key, T item) {
    std::unique_lock<std::mutex> l(a_.get()->m);
    DPRINTF("key=%d\n", (int)key);
    data_[key] = item;
    notify_all();
  }

  /// Get item by key. If not found until timeout, throw exception.
  T get(const KeyT key, const std::chrono::milliseconds timeout = 0ms) {
    auto* a = a_.get();
    std::unique_lock<decltype(a->m)> l(a->m);
    if (timeout > 0ms) {
      DPRINTF("key=%d\n", (int)key);
      if (!wait_until(
              &l, &a->cv, [&] { return (data_.find(key) != data_.end()); },
              timeout)) {
        throw std::runtime_error(std::string("Timeout after ") +
                                 std::to_string(timeout.count()));
      }
      return data_[key];
    }
    return data_[key];
  }
  bool empty() const { return size() == 0; }
  std::size_t size() const { return data_.size(); }

 private:
  std::map<KeyT, T> data_;
  std::mutex mut_;
};

}  // namespace async_map