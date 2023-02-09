#pragma once

#include "protocol/protocol.hpp"

#include "logging.hpp"
#include "ra2/abi.hpp"
#include "ra2yrproto/ra2yr.pb.h"
#include "utility/array_iterator.hpp"
#include "utility/serialize.hpp"

namespace ra2 {

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

}  // namespace ra2
