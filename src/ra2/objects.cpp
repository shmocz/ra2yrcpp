#include "ra2/objects.hpp"

using namespace ra2::objects;

ObjectClass::ObjectClass() {}
MissionClass::MissionClass() {}
RadioClass::RadioClass() {}
TechnoClass::TechnoClass() {}
BuildingClass::BuildingClass() {}
FootClass::FootClass() {}
UnitClass::UnitClass() {}
AircraftClass::AircraftClass() {}
InfantryClass::InfantryClass() {}
HouseClass::HouseClass() {}
ProgressTimer::ProgressTimer() {}
FactoryClass::FactoryClass() {}
#if 0
ObjectClass::ObjectClass(mem_buf ptr) : AbstractClass(ptr) { read(ptr); }

void ObjectClass::read(mem_buf& ptr) {
  ptr.read_value(health, offset);
  ptr.read_value(coords, offset + 11 * 0x4);
}

MissionClass::MissionClass(mem_buf ptr) : ObjectClass(ptr) {}
void MissionClass::read(mem_buf& ptr) {}
AbstractType MissionClass::id() const { return AbstractType::Abstract; }



RadioClass::RadioClass(mem_buf ptr) : MissionClass(ptr) {}
void RadioClass::read(mem_buf& ptr) {}
AbstractType RadioClass::id() const { return AbstractType::Abstract; }

TechnoClass::TechnoClass(mem_buf ptr) : RadioClass(ptr) { read(ptr); }
void TechnoClass::read(mem_buf& ptr) {
  ptr.read_value(owner, 0x21c);
  ptr.read_value(mind_controlled_by, 0x2c0);
  ptr.read_value(armor_multiplier, 0x158);
  ptr.read_value(firepower_multiplier, 0x160);
  ptr.read_value(shielded, 0x1d0);
  ptr.read_value(deactivated, 0x1d4);
}

#endif
