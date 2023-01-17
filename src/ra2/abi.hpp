#pragma once

#include "logging.hpp"
#include "ra2/game_screen.hpp"
#include "ra2/game_state.hpp"
#include "ra2/vectors.hpp"
#include "types.h"
#include "utility/function_traits.hpp"
#include "utility/memtools.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace ra2 {
namespace abi {

namespace {
using ra2::game_screen::CellClass;
}

class ABIGameMD {
 public:
  using deleter_t = std::function<void(void*)>;
  // JIT the functions here
  ABIGameMD();

  bool SelectObject(const u32 address) const;

  void SellBuilding(const u32 address) const;

  void DeployObject(const u32 address) const;

  bool ClickEvent(const u32 address, const u8 event) const;

  void sprintf(char** buf, const std::uintptr_t args_start) const;

  void ClickedMission(std::uintptr_t object, ra2::general::Mission m,
                      std::uintptr_t target_object, CellClass* target_cell,
                      CellClass* nearest_target_cell) const;

  bool BuildingClass_CanPlaceHere(std::uintptr_t p_this,
                                  vectors::CellStruct* cell,
                                  std::uintptr_t house_owner) const;

  void AddMessage(int id, const std::string message, const i32 color,
                  const i32 style, const u32 duration_frames,
                  bool single_player);

  u32 timeGetTime();

  template <typename CodeT, typename... Args>
  void add_entry(const std::uintptr_t address, Args... args) {
    code_generators_[address] = utility::make_uptr<CodeT>(address, args...);
  }

  template <typename E>
  void add_entry() {
    code_generators_[E::ptr] =
        utility::make_uptr<typename E::gen_t>(E::ptr, E::stack_size);
  }

  const std::map<u32, std::unique_ptr<void, deleter_t>>& code_generators()
      const;

 private:
  std::map<u32, std::unique_ptr<void, deleter_t>> code_generators_;
};

}  // namespace abi
}  // namespace ra2
