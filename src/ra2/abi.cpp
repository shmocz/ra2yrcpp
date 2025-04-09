#include "ra2/abi.hpp"

#include <cstring>

#include <stdexcept>
#include <string>

using namespace ra2::abi;

ABIGameMD::ABIGameMD() {}

Xbyak::CodeGenerator* ABIGameMD::find_codegen(u32 address) {
  try {
    return code_generators().at(address).get();
  } catch (const std::out_of_range& e) {
  }
  return nullptr;
}

codegen_store& ABIGameMD::code_generators() { return code_generators_; }

bool ABIGameMD::SelectObject(u32 address) {
  return ra2::abi::SelectObject::call(this, address);
}

void ABIGameMD::SellBuilding(u32 address) {
  ra2::abi::SellBuilding::call(this, address, 1);
}

void ABIGameMD::DeployObject(u32 address) {
  ra2::abi::DeployObject::call(this, address);
}

bool ABIGameMD::ClickEvent(u32 address, u8 event) {
  return ra2::abi::ClickEvent::call(this, address, event);
}

u32 ABIGameMD::timeGetTime() {
  return reinterpret_cast<u32 __stdcall (*)()>(
      serialize::read_obj_le<std::uintptr_t>(0x7E1530))();
}

bool ABIGameMD::BuildingTypeClass_CanPlaceHere(std::uintptr_t p_this,
                                               CellStruct* cell,
                                               std::uintptr_t house_owner) {
  return BuildingTypeClass_CanPlaceHere::call(this, p_this, cell, house_owner);
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

void ABIGameMD::AddMessage(int id, std::string message, i32 color, i32 style,
                           u32 duration_frames, bool single_player) {
  static constexpr std::uintptr_t MessageListClass = 0xA8BC60U;
  std::wstring m(message.begin(), message.end());
  ra2::abi::AddMessage::call(this, MessageListClass, nullptr, id, m.c_str(),
                             color, style, duration_frames, single_player);
}

int ra2::abi::get_tiberium_type(int overlayTypeIndex) {
  auto* A = OverlayTypeClass::Array.get();
  if (overlayTypeIndex == -1 || !A->GetItem(overlayTypeIndex)->Tiberium) {
    return -1;
  }
  auto I = DVCIterator(TiberiumClass::Array.get());
  for (const auto& t : I) {
    int ix = t->Image->ArrayIndex;
    if ((ix <= overlayTypeIndex && (overlayTypeIndex < t->NumImages + ix)) ||
        (t->NumImages + ix <= overlayTypeIndex &&
         (overlayTypeIndex < t->field_EC + t->NumImages + ix))) {
      return t->ArrayIndex;
    }
  }
  return 0;
}

int ra2::abi::get_tiberium_value(const CellClass& cell) {
  int ix = get_tiberium_type(cell.OverlayTypeIndex);
  if (ix == -1) {
    return 0;
  }
  return TiberiumClass::Array.get()->GetItem(ix)->Value *
         (cell.OverlayData + 1);
}
