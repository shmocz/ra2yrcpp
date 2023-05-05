#pragma once
#include <condition_variable>
#include <mutex>
#include <tuple>
#include <utility>

namespace util {

template <typename T, typename MutexT = std::mutex>
using acquire_t = std::tuple<std::unique_lock<MutexT>, T>;

/// Obtain exclusive access to resource @v guarded by mutex @m
template <typename T, typename MutexT = std::mutex, typename... MutexArgs>
inline auto acquire(MutexT& m, T* v, MutexArgs... args) {  // NOLINT
  return std::make_tuple(std::move(std::unique_lock<MutexT>(m, args...)), v);
}

template <typename FnT, typename MutexT = std::mutex>
auto guarded(MutexT& mut, FnT cb) {  // NOLINT
  std::unique_lock<MutexT> l(mut);
  return cb();
}

template <typename T, typename MutexT = std::mutex>
class AtomicVariable {
 public:
  explicit AtomicVariable(T value) : v_(value) {}

  void wait(T value) {
    std::unique_lock<MutexT> l(m_);
    cv_.wait(l, [this, value]() { return v_ == value; });
  }

  template <typename PredT>
  void wait_pred(PredT p) {
    std::unique_lock<MutexT> l(m_);
    cv_.wait(l, [this, p]() { return p(v_); });
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

  friend bool operator==(AtomicVariable<T>& lhs, T rhs) {  // NOLINT
    return lhs.get() == rhs;
  }

  friend bool operator!=(AtomicVariable<T>& lhs, T rhs) {  // NOLINT
    return !(lhs == rhs);
  }

 private:
  T v_;
  MutexT m_;
  std::condition_variable cv_;
};

}  // namespace util
