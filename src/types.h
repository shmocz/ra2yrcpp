#pragma once
#include <cstdint>

#include <chrono>
#include <vector>

#define X(s) typedef uint##s##_t u##s
X(8);
X(16);
X(32);
X(64);
#undef X
#define X(s) typedef int##s##_t i##s
X(8);
X(16);
X(32);
X(64);
#undef X

typedef std::uintptr_t addr_t;

union X86Regs {
  u32 regs[9];

  struct {
    u32 eflags, edi, esi, ebp, esp, ebx, edx, ecx, eax;
  };
};

enum class x86Reg : int {
  eax = 0,
  ebx = 1,
  ecx = 2,
  edx = 3,
  esi = 4,
  edi = 5,
  ebp = 6,
  esp = 7,
  eip = 8
};

using vecu8 = std::vector<unsigned char>;
using duration_t = std::chrono::duration<double>;
