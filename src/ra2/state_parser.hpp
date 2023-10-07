#pragma once

#include "ra2yrproto/ra2yr.pb.h"

#include "protocol/helpers.hpp"
#include "types.h"

#include <google/protobuf/repeated_ptr_field.h>

#include <cstddef>

#include <tuple>
#include <vector>
class CellClass;
class EventClass;
class HouseClass;
class MapClass;

namespace ra2 {
namespace abi {
class ABIGameMD;
}
}  // namespace ra2

namespace ra2 {

namespace pb = google::protobuf;

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
  const EventClass* src;
  ra2yrproto::ra2yr::Event* T;
  u32 time;
  EventParser(const EventClass* src, ra2yrproto::ra2yr::Event* T, u32 time);

  void Target();

  void MegaMission();

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

  static void copy_to(ra2yrproto::ra2yr::Cell* dst, const Cell* src);
};

void parse_MapData(ra2yrproto::ra2yr::MapData* dst, MapClass* src,
                   ra2::abi::ABIGameMD* abi);

void parse_EventLists(ra2yrproto::ra2yr::GameState* G,
                      ra2yrproto::ra2yr::EventListsSnapshot* ES,
                      std::size_t max_size);

void parse_prerequisiteGroups(ra2yrproto::ra2yr::PrerequisiteGroups* T);

void parse_map(std::vector<Cell>* previous, MapClass* D,
               pb::RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference);

std::vector<CellClass*> get_valid_cells(MapClass* M);

void parse_Factories(pb::RepeatedPtrField<ra2yrproto::ra2yr::Factory>* dst);

template <typename T, typename U>
static auto init_arrays(U* dst) {
  auto* D = T::Array.get();
  if (dst->size() != D->Count) {
    ra2yrcpp::protocol::fill_repeated_empty(dst, D->Count);
  }
  return std::make_tuple(D, dst);
}

pb::RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>*
parse_AbstractTypeClasses(
    pb::RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* T,
    ra2::abi::ABIGameMD* abi);

void parse_Objects(ra2yrproto::ra2yr::GameState* G, ra2::abi::ABIGameMD* abi);
void parse_HouseClasses(ra2yrproto::ra2yr::GameState* G);

ra2yrproto::ra2yr::ObjectTypeClass* find_type_class(
    pb::RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* types,
    ra2yrproto::ra2yr::AbstractType rtti_id, int array_index);

/// Return true if the current player is the only human player in the game.
bool is_local(const pb::RepeatedPtrField<ra2yrproto::ra2yr::House>& H);

}  // namespace ra2
