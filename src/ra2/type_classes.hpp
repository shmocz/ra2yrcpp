#pragma once
#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/vectors.hpp"

namespace ra2 {
namespace type_classes {
namespace {
using namespace ra2::general;
}

struct ObjectTypeClass : ra2::abstract_types::AbstractTypeClass {
  Armor armor;
  i32 strength;
};

struct TacticalClass : ra2::abstract_types::AbstractClass {};

struct TechnoTypeClass : ObjectTypeClass {
  i32 walkrate;
  i32 idlerate;
  float build_time_multiplier;
  i32 cost;
  i32 soylent;
  i32 flightlevel;
  i32 airstriketeam;
  i32 eliteairstriketeam;
  void* airstriketeamtype;
  void* eliteairstriketeamtype;
  i32 airstrikerechargetime;
  i32 eliteairstrikerechargetime;
  i32 threatposed;
  i32 points;
  i32 speed;
  SpeedType speed_type;
};

struct BuildingTypeClass : TechnoTypeClass {
  ra2::vectors::CellStruct* p_foundation_data;  // should contain width/height
  ra2::vectors::CellStruct foundation_data;
  i32 foundation;
};

struct AircraftTypeClass : TechnoTypeClass {};

struct UnitTypeClass : TechnoTypeClass {};

struct InfantryTypeClass : TechnoTypeClass {};

struct HouseTypeClass : TechnoTypeClass {};

}  // namespace type_classes
}  // namespace ra2
