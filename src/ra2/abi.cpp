#include "ra2/abi.hpp"

#include <xbyak/xbyak.h>

using namespace ra2::abi;

struct ThisCall : Xbyak::CodeGenerator {
  explicit ThisCall(const u32 p_fn, const size_t stack_size = 0u) {
    mov(ecx, ptr[esp + 0x4]);  // this

    // copy stack after this (push args)
    for (auto i = 0u; i * 4u < stack_size; i++) {
      mov(eax, ptr[esp + 0x4 + (stack_size - i * 4u)]);
      mov(ptr[esp - (i + 1) * 4u], eax);
    }
    sub(esp, stack_size);
    mov(eax, p_fn);  // target function
    call(eax);       // callee cleans the stack for __thiscall
    ret();           // stack size is maintained, we are safe to return
  }
};

template <typename T, typename D>
void put_entry(T* item, std::map<u32, std::unique_ptr<void, D>>* m, const u32 p,
               const std::size_t stack_size) {
  (*m)[p] = std::unique_ptr<void, ABIGameMD::deleter_t>(
      new ThisCall(p, stack_size),
      [](void* v) { delete reinterpret_cast<ThisCall*>(v); });
  *item = reinterpret_cast<ThisCall*>(m->at(p).get())->getCode<T>();
}

ABIGameMD::ABIGameMD() {
  put_entry(&SelectObject_, &code_generators_, game_state::p_SelectUnit, 0u);
  put_entry(&DeployObject_, &code_generators_, game_state::p_DeployObject, 0u);
  put_entry(&SellBuilding_, &code_generators_, game_state::p_SellBuilding, 0u);
  put_entry(&ClickEvent_, &code_generators_, game_state::p_ClickedEvent, 4u);
}

bool ABIGameMD::SelectObject(const u32 address) const {
  return SelectObject_(address);
}

void ABIGameMD::SellBuilding(const u32 address) const {
  SellBuilding_(address, 1);
}

void ABIGameMD::DeployObject(const u32 address) const {
  DeployObject_(address);
}

bool ABIGameMD::ClickEvent(const u32 address, const u8 event) const {
  return ClickEvent_(address, event);
}
