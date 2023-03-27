#pragma once

#include "protocol/protocol.hpp"

#include "logging.hpp"
#include "ra2/abi.hpp"
#include "ra2yrproto/ra2yr.pb.h"
#include "utility/array_iterator.hpp"
#include "utility/serialize.hpp"

#include <YRPP.h>
#include <google/protobuf/repeated_ptr_field.h>

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

void parse_MapData(ra2yrproto::ra2yr::MapData* dst, MapClass* src,
                   ra2::abi::ABIGameMD* abi);

void parse_EventLists(ra2yrproto::ra2yr::GameState* G,
                      ra2yrproto::ra2yr::EventListsSnapshot* ES,
                      std::size_t max_size);

void parse_prerequisiteGroups(ra2yrproto::ra2yr::PrerequisiteGroups* T);

}  // namespace ra2
