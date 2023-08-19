#include "process.hpp"
#include "utility.h"
#include "utility/time.hpp"

#include <gtest/gtest.h>

#include <cstddef>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

using namespace process;

class ThreadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    close = false;
    constexpr size_t num_threads = 3u;
    auto f = [this]() {
      while (!close) {
        util::sleep_ms(100);
      }
    };
    for (auto i = 0u; i < num_threads; i++) {
      threads.emplace_back(std::thread(f));
    }
  }

  void TearDown() override {
    close = true;
    for (auto& t : threads) {
      t.join();
    }
  }

  using pred_t = std::function<bool(Thread*, void*)>;

  std::vector<int> index2tid(process::Process* P, pred_t pred = nullptr) {
    std::vector<int> I;
    auto cb = [&pred](Thread* T, void* ctx) {
      if (pred == nullptr || pred(T, ctx)) {
        reinterpret_cast<decltype(&I)>(ctx)->push_back(T->id());
      }
    };
    P->for_each_thread(cb, &I);
    return I;
  }

  std::vector<std::thread> threads;
  std::atomic_bool close;
};

TEST_F(ThreadTest, TestProcessThreadIterationWorks) {
  auto P = process::get_current_process();
  auto tid = process::get_current_tid();
  std::vector<int> ix2tid = index2tid(&P);
// Mess around with suspend/resume
#if 1
  P.suspend_threads(tid);
  P.resume_threads(tid);
#endif
  ASSERT_EQ(ix2tid.size(), threads.size() + 1);
}

TEST_F(ThreadTest, TestProcessThreadIterationFilterWorks) {
  auto P = process::get_current_process();
  const int main_tid = get_current_tid();
  std::vector<int> ix2tid = index2tid(&P, [&main_tid](Thread* T, void* ctx) {
    (void)ctx;
    return T->id() != main_tid;
  });

  ASSERT_EQ(ix2tid.size(), threads.size());
  ASSERT_FALSE(yrclient::contains(ix2tid, main_tid));
}
