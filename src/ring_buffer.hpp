#pragma once

#include <cstddef>

#include <utility>
#include <vector>

namespace ring_buffer {

template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(const std::size_t max_size = -1u) : max_size_(max_size) {}

  void push(T t) { emplace(std::move(t)); }

  void emplace(T&& t) {
    if (size() >= max_size_) {
      q_.erase(q_.begin());
    }
    q_.emplace_back(std::move(t));
  }

  void pop() { q_.pop_back(); }

  T& front() { return q_.back(); }

  std::size_t size() const { return q_.size(); }

 private:
  std::vector<T> q_;
  std::size_t max_size_;
};
}  // namespace ring_buffer
