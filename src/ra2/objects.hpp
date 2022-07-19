#pragma once
#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/type_classes.hpp"
#include "ra2/vectors.hpp"

#include <string>

namespace ra2 {
namespace objects {

namespace {
using namespace ra2::general;
using namespace ra2::abstract_types;
}  // namespace

struct HouseClass : AbstractClass {
  i32 array_index;
  std::string name;
  std::string faction;
  ra2::type_classes::HouseTypeClass* house_type;
  ybool defeated;
  ybool current_player;
  i32 start_credits;
  i32 money;
  u32 self;
  HouseClass();
};

struct ObjectClass : public ra2::abstract_types::AbstractClass {
  static constexpr auto offset = 28 * 0x4;
  i32 health;
  u32 id;  // basically pointer to object
  ra2::vectors::CoordStruct coords;
  ObjectClass();
};

struct MissionClass : public ObjectClass {
  MissionClass();
};

struct RadioClass : public MissionClass {
  RadioClass();
};

struct TechnoClass : public RadioClass {
  HouseClass* owner;
  TechnoClass* mind_controlled_by;
  ra2::type_classes::TechnoTypeClass* p_type;
  double armor_multiplier;
  double firepower_multiplier;
  i32 shielded;
  ybool deactivated;
  TechnoClass();
};

struct BuildingClass : public TechnoClass {
  ra2::type_classes::BuildingTypeClass* building_type;
  BStateType build_state_type;
  BStateType queue_build_state;
  BuildingClass();
};

struct FootClass : public TechnoClass {
  double speed_percentage;
  double speed_multiplier;
  AbstractClass* destination;
  FootClass();
};

struct UnitClass : public FootClass {
  ra2::type_classes::UnitTypeClass* unit_type;
  UnitClass();
};

// NB!!! also inherits from FlasherClass
struct AircraftClass : public FootClass {
  ra2::type_classes::AircraftTypeClass* aircraft_type;
  AircraftClass();
};

struct InfantryClass : public FootClass {
  ra2::type_classes::InfantryTypeClass* infantry_type;
  InfantryClass();
};

struct ProgressTimer {
  i32 value;
  ProgressTimer();
};

// NB: the object is available in the global object DVC
struct FactoryClass : public AbstractClass {
  FactoryClass();
  ProgressTimer production;
  ra2::vectors::DynamicVectorClass<ra2::type_classes::TechnoTypeClass*> queue;
  HouseClass* owner;
  TechnoClass* object;
  vectors::DynamicVectorClass<type_classes::TechnoTypeClass*> queued_objects;
};

}  // namespace objects
}  // namespace ra2
