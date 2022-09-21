#pragma once

#include "ra2/game_state.hpp"
#include "types.h"

#include <functional>
#include <map>
#include <memory>

namespace ra2 {
namespace abi {

class ABIGameMD {
 public:
  using deleter_t = std::function<void(void*)>;
  // JIT the functions here
  ABIGameMD();

  bool __cdecl SelectObject(const u32 address) const;

  void __cdecl SellBuilding(const u32 address) const;

  void __cdecl DeployObject(const u32 address) const;

  bool __cdecl ClickEvent(const u32 address, const u8 event) const;

 private:
  bool __cdecl (*SelectObject_)(u32);
  void __cdecl (*SellBuilding_)(u32, u32);
  void __cdecl (*DeployObject_)(u32);
  bool __cdecl (*ClickEvent_)(u32, u8);

  std::map<u32, std::unique_ptr<void, deleter_t>> code_generators_;
};
}  // namespace abi
}  // namespace ra2
