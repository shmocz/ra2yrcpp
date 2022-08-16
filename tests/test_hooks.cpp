#include "debug_helpers.h"
#include "gtest/gtest.h"
#include "hook.hpp"
#include "process.hpp"
#include "utility/time.hpp"

#include <xbyak/xbyak.h>

#include <thread>
#include <vector>

using namespace process;
using namespace hook;
using namespace std;

/// Multiplies two unsigned integers, and returns result in EAX
struct ExampleProgram : Xbyak::CodeGenerator {
  ExampleProgram() {
    mov(eax, ptr[esp + 0x4]);
    mul(ptr[esp + 0x8]);
    ret();
  }
  static int expected(const unsigned a, const unsigned b) { return a * b; }
};

///
/// Program that stays in infinite loop, until ECX equals specific value.
size_t start_region(Xbyak::CodeGenerator* c, const unsigned int key) {
  using namespace Xbyak::util;
  c->push(key);
  c->mov(ecx, ptr[esp]);
  c->add(esp, 0x4);
  return c->getSize();
}
struct InfiniteLoop : Xbyak::CodeGenerator {
  size_t start_region_size;
  explicit InfiniteLoop(const unsigned int key) {
    L("L1");
    start_region_size = start_region(this, key);
    // Dummy instructions to decrease our chances of looping forever
    for (int i = 0; i < 20; i++) {
      mov(eax, ecx);
      cmp(eax, key);
    }
    je("L1");
    ret();
  }
  auto get_code() { return getCode<int __cdecl (*)()>(); }
};

int __cdecl add_ints(const int a, const int b) { return a + b; }

/// Sample function to test hooking on. Returns the number of bytes that need to
/// be copied to detour
size_t gen_add(Xbyak::CodeGenerator* c) {
  using namespace Xbyak::util;
  size_t sz = 8u;
  c->mov(eax, ptr[esp + 0x4]);
  c->add(eax, ptr[esp + 0x8]);
  c->ret();
  return sz;
}

TEST(HookTest, XbyakCodegenTest) {
  Xbyak::CodeGenerator C;
  (void)gen_add(&C);
  auto f = C.getCode<int __cdecl (*)(const int, const int)>();
  int c = 10;

  for (int i = 0; i < c; i++) {
    for (int j = 0; j < c; j++) {
      ASSERT_EQ(add_ints(i, j), f(i, j));
      ASSERT_EQ(f(j, i), f(i, j));
    }
  }
}

TEST(HookTest, BasicHookingWorks) {
  int a = 3;
  int b = 5;
  int res = add_ints(a, b);
  int cookie = 0u;
  auto my_cb = [](Hook* h, void* data, X86Regs* state) {
    (void)h;
    (void)state;
    int* p = reinterpret_cast<int*>(data);
    *p = 0xdeadbeef;
  };
  Hook::HookCallback cb{my_cb, &cookie};
  Xbyak::CodeGenerator C;
  size_t patch_size = gen_add(&C);
  auto f = C.getCode<int __cdecl (*)(const int, const int)>();
  hook::Hook H(reinterpret_cast<addr_t>(f), patch_size);
  H.add_callback(cb);
  ASSERT_EQ(res, f(a, b));
  ASSERT_EQ(cookie, 0xdeadbeef);
}

TEST(HookTest, TestCodeGeneration) {
  ExampleProgram C;
  int a = 10;
  int b = 13;
  auto f = C.getCode<int __cdecl (*)(const unsigned, const unsigned)>();
  ASSERT_EQ(f(a, b), C.expected(a, b));
}

// FIXME: inherently broken
TEST(HookTest, TestJumpLocationExampleCode) {
  GTEST_SKIP();
  auto P = process::get_current_process();
  const int key = 0xdeadbeef;
  InfiniteLoop C(key);
  auto f = C.getCode<int __cdecl (*)()>();
  auto t = std::thread(f);
  const int main_tid = get_current_tid();
  P.suspend_threads(main_tid);
  // Set key to different value
  // NB! this is broken -- no guarantee that thread is within our code
  P.for_each_thread([&main_tid](Thread* T, void* ctx) {
    (void)ctx;
    if (T->id() != main_tid) {
      T->set_gpr(x86Reg::ecx, 0);
    }
  });
  util::sleep_ms(5000);
  // Resume threads
  P.resume_threads(main_tid);
  t.join();
}

TEST(HookTest, BasicCallbackMultipleThreads) {
  const int key = 0xdeadbeef;
  const size_t num_threads = 3;
  // Create test function
  InfiniteLoop C(key);
  // TODO: use this pattern everywhere
  auto f = C.get_code();

  // Callback which allows the thread to exit the infinite loop
  auto cb_f = [&key](Hook* h, void* data, X86Regs* state) {
    (void)h;
    (void)data;
    state->ecx = 0;
  };
  Hook::HookCallback cb{cb_f, nullptr};

  // spawn threads
  vector<thread> threads;
  for (auto i = 0u; i < num_threads; i++) {
    threads.emplace_back(thread(f));
  }
  // create the hook with detour trampoline
  Hook H(reinterpret_cast<addr_t>(f), C.start_region_size);
  // add callbacks
  H.add_callback(cb);

  // join threads & do sanity check
  for (auto& t : threads) {
    t.join();
  }
}

TEST(HookTest, CorrectBehaviorWhenThreadsInHook) {}

TEST(HookTest, CorrectBehaviorInAllScenarios) {}
