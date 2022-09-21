#include "ra2/abi.hpp"

#include <xbyak/xbyak.h>

using namespace ra2::abi;

struct ThisCall : Xbyak::CodeGenerator {
  explicit ThisCall(const u32 p_fn) {
    mov(ecx, ptr[esp + 0x4]);  // this
    mov(eax, ptr[esp]);        // return address
    mov(ptr[esp + 0x4], eax);
    mov(eax, p_fn);  // target function
    mov(ptr[esp], eax);
    // add(esp, 0x4);  // adjust stack since we removed &this
    ret();  // call target function
  }
};

template <typename T, typename D>
void put_entry(T* item, std::map<u32, std::unique_ptr<void, D>>* m,
               const u32 p) {
  (*m)[p] = std::unique_ptr<void, ABIGameMD::deleter_t>(
      new ThisCall(p), [](void* v) { delete reinterpret_cast<ThisCall*>(v); });
  *item = reinterpret_cast<ThisCall*>(m->at(p).get())->getCode<T>();
}

ABIGameMD::ABIGameMD() {
  put_entry(&SelectObject_, &code_generators_, game_state::p_SelectUnit);
  put_entry(&DeployObject_, &code_generators_, game_state::p_DeployObject);
  put_entry(&SellBuilding_, &code_generators_, game_state::p_SellBuilding);
  put_entry(&ClickEvent_, &code_generators_, game_state::p_ClickedEvent);
}

bool ABIGameMD::SelectObject(const u32 address) const {
  return SelectObject_(address);
}

void ABIGameMD::SellBuilding(const u32 address) const {
  return SellBuilding_(address, 1);
}

void ABIGameMD::DeployObject(const u32 address) const {
  DeployObject_(address);
}

bool ABIGameMD::ClickEvent(const u32 address, const u8 event) const {
  return ClickEvent_(address, event);
}
