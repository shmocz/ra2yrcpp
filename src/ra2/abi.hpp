#pragma once

#include "logging.hpp"
#include "types.h"
#include "utility/function_traits.hpp"
#include "utility/memtools.hpp"
#include "utility/serialize.hpp"

#include <YRPP.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

// Forward decl
class CellClass;
class BuildingTypeClass;

namespace ra2 {
namespace abi {

class ABIGameMD {
 public:
  using deleter_t = std::function<void(void*)>;
  // JIT the functions here
  ABIGameMD();

  bool SelectObject(const u32 address);

  void SellBuilding(const u32 address);

  void DeployObject(const u32 address);

  bool ClickEvent(const u32 address, const u8 event);

  void sprintf(char** buf, const std::uintptr_t args_start);

  void ClickedMission(std::uintptr_t object, Mission m,
                      std::uintptr_t target_object, CellClass* target_cell,
                      CellClass* nearest_target_cell);

  bool BuildingClass_CanPlaceHere(std::uintptr_t p_this, CellStruct* cell,
                                  std::uintptr_t house_owner);

  void AddMessage(int id, const std::string message, const i32 color,
                  const i32 style, const u32 duration_frames,
                  bool single_player);

  AbstractType AbstractClass_WhatAmI(AbstractClass* object);

  int CellClass_GetContainedTiberiumValue(std::uintptr_t p_this);

  u32 timeGetTime();

  bool DisplayClass_Passes_Proximity_Check(std::uintptr_t p_this,
                                           BuildingTypeClass* p_object,
                                           u32 house_index, CellStruct* cell);

  template <typename CodeT, typename... Args>
  void add_entry(const std::uintptr_t address, Args... args) {
    code_generators_[address] = ::utility::make_uptr<CodeT>(address, args...);
  }

  template <typename CodeT, typename... Args>
  void add_virtual(int index, const std::uintptr_t address, Args... args) {
    code_generators_[address] = ::utility::make_uptr<CodeT>(index, args...);
  }

  // FIXME: is this unused?
  template <typename E>
  void add_entry() {
    code_generators_[E::ptr] =
        ::utility::make_uptr<typename E::gen_t>(E::ptr, E::stack_size);
  }

  std::map<u32, std::unique_ptr<void, deleter_t>>& code_generators();

 private:
  std::map<u32, std::unique_ptr<void, deleter_t>> code_generators_;
};

}  // namespace abi
}  // namespace ra2
