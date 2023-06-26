#pragma once

#include "protocol/protocol.hpp"

#include "ra2/abi.hpp"
#include "ra2yrproto/ra2yr.pb.h"

#include <google/protobuf/repeated_ptr_field.h>

#include <YRPP.h>
#include <algorithm>
#include <vector>

namespace ra2 {

using google::protobuf::RepeatedPtrField;

struct Cookie {
  ra2::abi::ABIGameMD* abi;
  void* src;
};

struct ClassParser {
  Cookie c;
  ra2yrproto::ra2yr::Object* T;

  ClassParser(Cookie c, ra2yrproto::ra2yr::Object* T);

  void Object();

  void Mission();

  void Radio();

  void Techno();

  void Foot();

  void Aircraft();

  void Unit();

  void Building();

  void Infantry();

  void parse();
};

struct TypeClassParser {
  Cookie c;
  ra2yrproto::ra2yr::ObjectTypeClass* T;

  TypeClassParser(Cookie c, ra2yrproto::ra2yr::ObjectTypeClass* T);

  void AbstractType();

  void AircraftType();

  void TechnoType();

  void UnitType();

  void InfantryType();

  void ObjectType();

  void BuildingType();

  void parse();
};

struct EventParser {
  EventClass* src;
  ra2yrproto::ra2yr::Event* T;
  u32 time;
  EventParser(EventClass* src, ra2yrproto::ra2yr::Event* T, u32 time);

  void Target();

  void MegaMission();

  void MegaMission_F();

  void Production();

  void Place();

  void parse();
};

void parse_HouseClass(ra2yrproto::ra2yr::House* dst, const HouseClass* src);

// Intermediate structure for more efficient map data processing
struct Cell {
  i32 land_type;
  // crate type and some weird data
  i32 overlay_data;
  i32 tiberium_value;
  // Objects in this Cell
  // repeated Object objects = 8;
  double radiation_level;
  u32 passability;
  int index;
  bool shrouded;
  char height;
  char level;
  char pad[1];

  static void copy_to(ra2yrproto::ra2yr::Cell* dst, const Cell* src) {
    dst->set_land_type(
        static_cast<ra2yrproto::ra2yr::LandType>(src->land_type));
    dst->set_radiation_level(src->radiation_level);
    dst->set_height(static_cast<i32>(src->height));
    dst->set_level(static_cast<i32>(src->level));
    dst->set_overlay_data(src->overlay_data);
    dst->set_tiberium_value(src->tiberium_value);
    dst->set_shrouded(src->shrouded);
    dst->set_passability(src->passability);
    dst->set_index(src->index);
  }
};

void parse_MapData(ra2yrproto::ra2yr::MapData* dst, MapClass* src,
                   ra2::abi::ABIGameMD* abi);

void parse_EventLists(ra2yrproto::ra2yr::GameState* G,
                      ra2yrproto::ra2yr::EventListsSnapshot* ES,
                      std::size_t max_size);

void parse_prerequisiteGroups(ra2yrproto::ra2yr::PrerequisiteGroups* T);

void parse_map(std::vector<Cell>* previous, MapClass* D,
               RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference);

std::vector<CellClass*> get_valid_cells(MapClass* M);

}  // namespace ra2
