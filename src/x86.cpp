#include "x86.hpp"

#include "types.h"

#include <xbyak/xbyak.h>

#include <algorithm>
#include <utility>
#include <vector>

using namespace x86;

std::size_t x86::bytes_to_stack(Xbyak::CodeGenerator* c, const vecu8 bytes) {
  using namespace Xbyak::util;
  int off = sizeof(u32);
  std::size_t s = 0u;
  auto it = bytes.begin();

  std::vector<u32> chunks;
  for (std::size_t i = 0; i < bytes.size(); i += off) {
    u32 dw{0};
    std::copy(it + i, it + std::min(bytes.size(), i + off),
              reinterpret_cast<u8*>(&dw));
    chunks.push_back(dw);
  }
  // push dw:s in reverse order
  for (auto i2 = chunks.rbegin(); i2 != chunks.rend(); i2++) {
    c->push(*i2);
    s += off;
  }
  return s;
}

void x86::restore_regs(Xbyak::CodeGenerator* c) {
#ifdef XBYAK32
  c->popfd();
  c->popad();
#elif defined(XBYAK64)
  c->popfq();
#endif
}

void x86::save_regs(Xbyak::CodeGenerator* c) {
#ifdef XBYAK32
  c->pushad();
  c->pushfd();
#elif defined(XBYAK64)
  c->pushfq();
#endif
}
