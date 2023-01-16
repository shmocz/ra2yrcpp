#pragma once
#include "ra2/general.h"

#include <string>

namespace ra2 {
namespace abstract_types {

namespace {
using namespace ra2::general;
}

struct AbstractClass {
  AbstractType type_id;
  AbstractFlags flags;
};

struct AbstractTypeClass : public AbstractClass {
  u32 p_vtable;
  std::string name;
  i32 array_index;  // non standard
};

struct NewAbstractClass {
  void* vtable;
  u32 UniqueID;  // generated by IRTTIInfo::Create_ID through an amazingly
                 // simple sequence of return
                 // ++ScenarioClass::Instance->UniqueID;
  AbstractFlags
      AbstractFlags;  // flags, see AbstractFlags enum in GeneralDefinitions.
  u32 unknown_18;     // 0x18
  i32 RefCount;       // 0x1c
  bool Dirty;         // 0x20 for IPersistStream.
  u8 padding_21[0x3];
};

}  // namespace abstract_types
}  // namespace ra2
