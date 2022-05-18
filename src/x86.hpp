#pragma once
#include "types.h"
#include <xbyak/xbyak.h>
#include <algorithm>
#include <vector>

namespace x86 {

size_t bytes_to_stack(Xbyak::CodeGenerator* c, const vecu8 bytes);
std::vector<Xbyak::Reg32> get_regs(const Xbyak::CodeGenerator& c);
void restore_regs(Xbyak::CodeGenerator* c);
void save_regs(Xbyak::CodeGenerator* c);

template <typename T, typename B, typename E>
inline T from_buf(B b, E e) {
  T t{0};
  auto p = reinterpret_cast<u8*>(&t);
  copy(b, e, p);
  return t;
}
};  // namespace x86
