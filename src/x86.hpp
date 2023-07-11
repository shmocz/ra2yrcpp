#pragma once
#include "types.h"

#include <xbyak/xbyak.h>

#include <algorithm>
#include <vector>

namespace x86 {

size_t bytes_to_stack(Xbyak::CodeGenerator* c, const vecu8 bytes);
void restore_regs(Xbyak::CodeGenerator* c);
void save_regs(Xbyak::CodeGenerator* c);

};  // namespace x86
