#pragma once
#include "async_queue.hpp"
#include "logging.hpp"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <ratio>
#include <string>

namespace async_map {

namespace {
using namespace std::chrono_literals;
}

template <typename PredT, typename LockT = std::mutex>
bool wait_until(std::unique_lock<LockT>* lock, std::condition_variable* cv,
                PredT pred, const duration_t timeout = 0.0s) {
  return (cv->wait_for(*lock, timeout, pred));
}

// TODO(shmocz): size limit
// TODO(shmocz): This is only used in multi_client. Could re-use elsewhere.
template <typename T, typename KeyT = std::uint64_t>
class AsyncMap : public async_queue::AsyncContainer {
 public:
  AsyncMap() = default;

  void put(const KeyT key, T item) {
    std::unique_lock<std::mutex> l(a_.get()->m);
    auto [it, status] = data_.try_emplace(key, item);
    if (!status) {
      throw std::runtime_error(fmt::format("key exists: {}", key));
    }
    notify_all();
  }

  /// Get item by key. If not found until timeout, throw exception.
  T get(const KeyT key, const duration_t timeout = 0.0s) {
    auto* a = a_.get();
    std::unique_lock<decltype(a->m)> l(a->m);
    if (timeout > 0.0s) {
      if (!wait_until(
              &l, &a->cv, [&] { return (data_.find(key) != data_.end()); },
              timeout)) {
        throw std::runtime_error("timeout after " +
                                 std::to_string(timeout.count()) + "ms");
      }
      return data_.at(key);
    }
    return data_.at(key);
  }

  void erase(const KeyT key) {
    auto* a = a_.get();
    std::unique_lock<decltype(a->m)> l(a->m);
    data_.erase(key);
  }

  bool empty() const { return size() == 0; }

  std::size_t size() const { return data_.size(); }

 private:
  std::map<KeyT, T> data_;
  std::mutex mut_;
};

}  // namespace async_map
