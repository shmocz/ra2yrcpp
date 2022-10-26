#include "debug_helpers.h"
#include "process.hpp"
#include "utility.h"
#include "utility/scope_guard.hpp"
#include "utility/time.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace process;
using namespace std;

class ThreadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    constexpr size_t num_threads = 3u;
    auto f = [](std::chrono::milliseconds d) { util::sleep_ms(d); };
    for (auto i = 0u; i < num_threads; i++) {
      threads.emplace_back(thread(f, std::chrono::milliseconds((i + 1) * 500)));
    }
  }

  void TearDown() override {
    for (auto& t : threads) {
      t.join();
    }
  }

  vector<thread> threads;
};

TEST_F(ThreadTest, TestProcessThreadIterationWorks) {
  auto P = process::get_current_process();
  auto tid = process::get_current_tid();
  vector<int> index2tid;
  auto cb = [](Thread* T, void* ctx) {
    auto d = reinterpret_cast<decltype(&index2tid)>(ctx);
    d->push_back(T->id());
  };
  P.for_each_thread(cb, &index2tid);
// Mess around with suspend/resume
#if 1
  P.suspend_threads(tid);
  P.resume_threads(tid);
#endif
  ASSERT_EQ(index2tid.size(), threads.size() + 1);
}

TEST_F(ThreadTest, TestProcessThreadIterationFilterWorks) {
  auto P = process::get_current_process();
  vector<int> index2tid;
  const int main_tid = get_current_tid();
  auto cb = [&main_tid](Thread* T, void* ctx) {
    auto d = reinterpret_cast<decltype(&index2tid)>(ctx);
    if (T->id() != main_tid) {
      d->push_back(T->id());
    }
  };
  P.for_each_thread(cb, &index2tid);
  ASSERT_EQ(index2tid.size(), threads.size());
  ASSERT_FALSE(yrclient::contains(index2tid, main_tid));
}
