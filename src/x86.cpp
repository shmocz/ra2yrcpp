#include "x86.hpp"

#include "types.h"

#include <xbyak/xbyak.h>

#include <algorithm>
#include <utility>
#include <vector>

using namespace x86;

size_t x86::bytes_to_stack(Xbyak::CodeGenerator* c, const vecu8 bytes) {
  using namespace Xbyak::util;
  int off = sizeof(u32);
  size_t s = 0u;
  auto it = bytes.begin();

  std::vector<u32> chunks;
  for (size_t i = 0; i < bytes.size(); i += off) {
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

static std::vector<Xbyak::Reg32> get_regs(const Xbyak::CodeGenerator& c) {
  return {c.eax, c.ebx, c.ecx, c.edx, c.esi, c.edi, c.ebp, c.esp};
}

void x86::restore_regs(Xbyak::CodeGenerator* c) {
#ifdef XBYAK32
  c->popfd();
#elif defined(XBYAK64)
  c->popfq();
#endif
  for (auto r : get_regs(*c)) {
    c->pop(r);
  }
}

void x86::save_regs(Xbyak::CodeGenerator* c) {
  auto regs = get_regs(*c);
  for (auto r = regs.rbegin(); r != regs.rend(); r++) {
    c->push(*r);
  }
#ifdef XBYAK32
  c->pushfd();
#elif defined(XBYAK64)
  c->pushfq();
#endif
}
