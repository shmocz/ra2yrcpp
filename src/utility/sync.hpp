#pragma once
#include "types.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <tuple>
#include <utility>

namespace util {

namespace {
using namespace std::chrono_literals;
}

template <typename T, typename MutexT>
class AcquireData {
 public:
  using data_t = std::tuple<std::unique_lock<MutexT>, T*>;

  AcquireData(T* data, MutexT* m)
      : data_(std::make_tuple(std::move(std::unique_lock<MutexT>(*m)), data)) {}

  AcquireData(T* data, MutexT* m, const duration_t timeout)
      : data_(std::make_tuple(std::move(std::unique_lock<MutexT>(*m, timeout)),
                              data)) {}

  ~AcquireData() {}

  void unlock() { std::get<0>(data()).unlock(); }

  // Move constructor
  AcquireData(AcquireData&& other) noexcept : data_(std::move(other.data_)) {}

  // Move assignment operator
  AcquireData& operator=(AcquireData&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
    }
    return *this;
  }

  AcquireData(const AcquireData&) = delete;
  AcquireData& operator=(const AcquireData&) = delete;

  data_t& data() { return data_; }

 private:
  std::tuple<std::unique_lock<MutexT>, T*> data_;
};

template <typename T, typename MutexT = std::mutex>
using acquire_t = std::tuple<AcquireData<T, MutexT>, T*>;

template <typename T, typename MutexT, typename... Args>
static acquire_t<T, MutexT> acquire(T* data, MutexT* mut, Args... args) {
  return std::make_tuple(std::move(AcquireData(data, mut, args...)), data);
}

template <typename T, typename MutexT = std::mutex>
class AtomicVariable {
 public:
  explicit AtomicVariable(T value) : v_(value) {}

  void wait(const T value, const duration_t timeout = 0.0s) {
    wait_pred([value](const auto v) { return v == value; }, timeout);
  }

  template <typename PredT>
  void wait_pred(PredT p, const duration_t timeout = 0.0s) {
    std::unique_lock<MutexT> l(m_);
    if (timeout > 0.0s) {
      cv_.wait_for(l, timeout, [this, p]() { return p(v_); });
    } else {
      cv_.wait(l, [this, p]() { return p(v_); });
    }
  }

  void store(T v) {
    std::unique_lock<MutexT> l(m_);
    v_ = v;
    cv_.notify_all();
  }

  T get() {
    std::unique_lock<MutexT> l(m_);
    return v_;
  }

  friend bool operator==(AtomicVariable<T, MutexT>& lhs, T rhs) {  // NOLINT
    return lhs.get() == rhs;
  }

  friend bool operator!=(AtomicVariable<T, MutexT>& lhs, T rhs) {  // NOLINT
    return !(lhs == rhs);
  }

  auto acquire() { return util::acquire(&v_, &m_); }

 private:
  T v_;
  MutexT m_;
  std::condition_variable cv_;
};

}  // namespace util
