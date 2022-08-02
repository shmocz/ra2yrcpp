#pragma once
#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/vectors.hpp"

#include <vector>

namespace ra2 {
namespace type_classes {
namespace {
using namespace ra2::general;
}

struct ObjectTypeClass : ra2::abstract_types::AbstractTypeClass {
  Armor armor;
  i32 strength;
  u32 pointer_self;
};

struct TacticalClass : ra2::abstract_types::AbstractClass {};

struct SHPStruct {
  void* ptr;
  i16 width;
  i16 height;
  i16 frames;
  std::vector<std::vector<u8>> pixel_data;
};

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
  void* p_cameo;
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
