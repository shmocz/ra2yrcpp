#include "gtest/gtest.h"
#include "hook.hpp"
#include <xbyak/xbyak.h>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <iostream>
#include <thread>

using namespace hook;
using namespace std;

int __cdecl add_ints(const int a, const int b) { return a + b; }

void DEBUG_WAIT() {
  auto d = 10000ms;
  cerr << "WAIT " << endl;
  std::this_thread::sleep_for(d);
}

/// Sample function to test hooking on. Returns the number of bytes that need to
/// be copied to detour
size_t gen_add(Xbyak::CodeGenerator& c) {
  using namespace Xbyak::util;
  size_t sz = 8u;
  c.mov(eax, ptr[esp + 0x4]);
  c.add(eax, ptr[esp + 0x8]);
  c.ret();
  return sz;
}

TEST(HookTest, XbyakCodegenTest) {
  Xbyak::CodeGenerator C;
  (void)gen_add(C);
  auto f = C.getCode<int __cdecl (*)(const int, const int)>();
  int c = 10;

  for (int i = 0; i < c; i++) {
    for (int j = 0; j < c; j++) {
      ASSERT_EQ(add_ints(i, j), f(i, j));
      ASSERT_EQ(f(j, i), f(i, j));
    }
  }
}

void __cdecl my_callback(void* hook_object, void* user_data, X86Regs state) {
  (void)hook_object;
  (void)user_data;
  int* p = reinterpret_cast<int*>(user_data);
  *p = 0xdeadbeef;
}

TEST(HookTest, BasicHookingWorks) {
  int a = 3;
  int b = 5;
  int res = add_ints(a, b);
  int cookie = 0u;
  UserCallback cb{&my_callback, &cookie};
  Xbyak::CodeGenerator C;
  size_t patch_size = gen_add(C);
  auto f = C.getCode<int __cdecl (*)(const int, const int)>();
  hook::Hook H(reinterpret_cast<addr_t>(f), patch_size);
  H.add_callback(cb);
  ASSERT_EQ(res, f(a, b));
  ASSERT_EQ(cookie, 0xdeadbeef);
}