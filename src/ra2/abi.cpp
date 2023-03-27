#include "ra2/abi.hpp"

using namespace ra2::abi;

ABIGameMD::ABIGameMD() {}

std::map<u32, std::unique_ptr<void, deleter_t>>& ABIGameMD::code_generators() {
  return code_generators_;
}

util::acquire_t<codegen_t*, std::recursive_mutex>
ABIGameMD::acquire_code_generators() {
  return util::acquire(mut_code_generators_, &code_generators_);
}

bool ABIGameMD::SelectObject(const u32 address) {
  return ra2::abi::SelectObject::call(this, address);
}

void ABIGameMD::SellBuilding(const u32 address) {
  ra2::abi::SellBuilding::call(this, address, 1);
}

void ABIGameMD::DeployObject(const u32 address) {
  ra2::abi::DeployObject::call(this, address);
}

bool ABIGameMD::ClickEvent(const u32 address, const u8 event) {
  return ra2::abi::ClickEvent::call(this, address, event);
}

void ABIGameMD::sprintf(char** buf, const std::uintptr_t args_start) {
  char fake_stack[128];
  auto val = ::utility::asint(buf);
  std::memset(&fake_stack[0], 0, sizeof(fake_stack));
  std::memcpy(&fake_stack[0], &val, 4U);
  // copy rest of the args (WARNING: out of bounds read)
  std::memcpy(&fake_stack[4], ::utility::asptr<char*>(args_start),
              sizeof(fake_stack) - 4U);
  ra2::abi::sprintf::call(this, &fake_stack[0], sizeof(fake_stack));
}

u32 ABIGameMD::timeGetTime() {
  return reinterpret_cast<u32 __stdcall (*)()>(
      serialize::read_obj_le<std::uintptr_t>(0x7E1530))();
}

bool ABIGameMD::BuildingClass_CanPlaceHere(std::uintptr_t p_this,
                                           CellStruct* cell,
                                           std::uintptr_t house_owner) {
  return BuildingClass_CanPlaceHere::call(this, p_this, cell, house_owner);
}

bool ABIGameMD::DisplayClass_Passes_Proximity_Check(std::uintptr_t p_this,
                                                    BuildingTypeClass* p_object,
                                                    u32 house_index,
                                                    CellStruct* cell) {
  auto* fnd = BuildingTypeClass_GetFoundationData::call(this, p_object, true);

  return ra2::abi::DisplayClass_Passes_Proximity_Check::call(
             this, p_this, p_object, house_index, fnd, cell) &&
         ra2::abi::DisplayClass_SomeCheck::call(this, p_this, p_object,
                                                house_index, fnd, cell);
}

void ABIGameMD::AddMessage(int id, const std::string message, const i32 color,
                           const i32 style, const u32 duration_frames,
                           bool single_player) {
  static constexpr std::uintptr_t MessageListClass = 0xA8BC60U;
  std::wstring m(message.begin(), message.end());
  ra2::abi::AddMessage::call(this, MessageListClass, nullptr, id, m.c_str(),
                             color, style, duration_frames, single_player);
}
