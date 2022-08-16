#include "ra2/game_state.hpp"

#include <xbyak/xbyak.h>

using namespace ra2::game_state;

void GameState::add_AbstractTypeClass(std::unique_ptr<AbstractTypeClass> a,
                                      const std::uintptr_t* real_address) {
  // FIXME: use and fix contains
  if (abstract_type_classes_.find((u32)real_address) !=
      abstract_type_classes_.end()) {
    throw std::runtime_error("duplicate key");
  }

  abstract_type_classes_.try_emplace((u32)real_address, std::move(a));
}

void GameState::add_HouseClass(std::unique_ptr<objects::HouseClass> h) {
  house_classes_.emplace_back(std::move(h));
}

void GameState::add_FactoryClass(std::unique_ptr<objects::FactoryClass> f) {
  factory_classes_.emplace_back(std::move(f));
}

atc_map_t& GameState::abstract_type_classes() { return abstract_type_classes_; }

houseclass_vec_t& GameState::house_classes() { return house_classes_; }

factoryclass_vec_t& GameState::factory_classes() { return factory_classes_; }

struct ThisCall : Xbyak::CodeGenerator {
  ThisCall() {
    mov(ecx, ptr[esp + 0x8]);  // this
    mov(eax, ptr[esp]);        // return address
    mov(ptr[esp + 0x8], eax);
    mov(eax, ptr[esp + 0x4]);  // target function
    mov(ptr[esp + 0x4], eax);
    add(esp, 0x4);  // adjust stack since we removed &this
    ret();          // call target function
  }
};

bool ra2::game_state::SelectObject(const std::uint32_t address) {
  typedef bool __cdecl (*fn_ThisCall_t)(u32, u32);
  static ThisCall C;
  // game_state::p_SelectUnit
  static auto* fn = C.getCode<fn_ThisCall_t>();
  return fn(p_SelectUnit, address);
}

void ra2::game_state::SellBuilding(const std::uint32_t address) {
  typedef void __cdecl (*fn_ThisCall_t)(u32, u32, u32);
  static ThisCall C;
  static auto* fn = C.getCode<fn_ThisCall_t>();
  return fn(game_state::p_SellBuilding, address, 1);
}

void ra2::game_state::DeployObject(const std::uint32_t address) {
  typedef void __cdecl (*fn_ThisCall_t)(u32, u32);
  static ThisCall C;
  static auto* fn = C.getCode<fn_ThisCall_t>();
  return fn(game_state::p_DeployObject, address);
}
