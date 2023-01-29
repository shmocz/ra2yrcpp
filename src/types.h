#pragma once
#include <cstdint>

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

typedef u32 addr_t;

union X86Regs {
  u32 regs[9];

  struct {
    u32 eflags, eax, ebx, ecx, edx, esi, edi, ebp, esp;
  };
};

using vecu8 = std::vector<unsigned char>;
