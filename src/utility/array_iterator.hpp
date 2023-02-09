#pragma once
#include <iterator>

namespace utility {
template <typename T>
struct ArrayIterator {
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = value_type*;
    using reference = value_type&;

    explicit Iterator(T* ptr) : ptr_(ptr) {}

    reference operator*() const { return *ptr_; }

    pointer operator->() { return ptr_; }

    // Prefix increment
    Iterator& operator++() {
      ++ptr_;
      return *this;
    }

    // Postfix increment
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.ptr_ == b.ptr_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return a.ptr_ != b.ptr_;
    }

   private:
    T* ptr_;
  };

  explicit ArrayIterator(T* begin, std::size_t count)
      : v(begin), count(count) {}

  Iterator begin() { return Iterator(&v[0]); }

  Iterator end() { return Iterator(&v[count]); }

  T* v;
  std::size_t count;
};
}  // namespace utility
