#include "ra2/state_parser.hpp"

#include <YRPP.h>

using namespace ra2;

ClassParser::ClassParser(Cookie c, ra2yrproto::ra2yr::Object* T) : c(c), T(T) {}

void ClassParser::Object() {
  auto* P = reinterpret_cast<ObjectClass*>(c.src);
  T->set_health(P->Health);
  T->set_selected(P->IsSelected);
}

void ClassParser::Mission() { Object(); }

void ClassParser::Radio() { Mission(); }

void ClassParser::Techno() {
  Radio();
  auto* q = T->mutable_coordinates();
  auto* P = reinterpret_cast<TechnoClass*>(c.src);
  T->set_pointer_house(reinterpret_cast<u32>(P->Owner));
  T->set_pointer_initial_owner(reinterpret_cast<u32>(P->InitialOwner));
  // TODO: armor multiplier
  auto L = P->Location;
  q->set_x(L.X);
  q->set_y(L.Y);
  q->set_z(L.Z);
}

void ClassParser::Foot() { Techno(); }

void ClassParser::Aircraft() {
  Foot();
  auto* P = reinterpret_cast<AircraftClass*>(c.src);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_AIRCRAFT);
}

void ClassParser::Unit() {
  Foot();
  auto* P = reinterpret_cast<UnitClass*>(c.src);
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_UNIT);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
  T->set_deployed(P->Deployed);
  T->set_deploying(P->IsDeploying);
}

void ClassParser::Building() {
  Techno();
  auto* P = reinterpret_cast<BuildingClass*>(c.src);
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_BUILDING);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
}

void ClassParser::Infantry() {
  Foot();
  auto* P = reinterpret_cast<InfantryClass*>(c.src);
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_INFANTRY);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
}

void ClassParser::parse() {
  T->set_pointer_self(reinterpret_cast<u32>(c.src));
  auto t =
      c.abi->AbstractClass_WhatAmI(reinterpret_cast<AbstractClass*>(c.src));

  if (t == UnitClass::AbsID) {
    Unit();
  } else if (t == BuildingClass::AbsID) {
    Building();
  } else if (t == InfantryClass::AbsID) {
    Infantry();
  } else if (t == AircraftClass::AbsID) {
    Aircraft();
  } else {
    eprintf("unknown ObjectClass: {}", static_cast<int>(t));
  }
}

TypeClassParser::TypeClassParser(Cookie c,
                                 ra2yrproto::ra2yr::ObjectTypeClass* T)
    : c(c), T(T) {}

void TypeClassParser::AbstractType() {
  auto* P = reinterpret_cast<AbstractTypeClass*>(c.src);
  T->set_name(P->Name);
}

void TypeClassParser::AircraftType() { TechnoType(); }

void TypeClassParser::TechnoType() {
  ObjectType();
  auto* P = reinterpret_cast<TechnoTypeClass*>(c.src);
  T->set_cost(P->Cost);
  T->set_soylent(P->Soylent);
  auto A =
      utility::ArrayIterator<int>(P->Prerequisite.Items, P->Prerequisite.Count);
  for (auto r : A) {
    T->add_prerequisites(r);
  }
}

void TypeClassParser::UnitType() { TechnoType(); }

void TypeClassParser::InfantryType() { TechnoType(); }

void TypeClassParser::ObjectType() {
  AbstractType();
  auto* P = reinterpret_cast<ObjectTypeClass*>(c.src);
  T->set_armor_type(static_cast<ra2yrproto::ra2yr::Armor>(P->Armor));
}

void TypeClassParser::BuildingType() {
  TechnoType();
  auto* P = reinterpret_cast<BuildingTypeClass*>(c.src);
  T->set_array_index(P->ArrayIndex);
}

void TypeClassParser::parse() {
  T->set_pointer_self(reinterpret_cast<u32>(c.src));
  auto t =
      c.abi->AbstractClass_WhatAmI(reinterpret_cast<AbstractClass*>(c.src));
  if (t == BuildingTypeClass::AbsID) {
    BuildingType();
  } else if (t == AircraftTypeClass::AbsID) {
    AircraftType();
  } else if (t == InfantryTypeClass::AbsID) {
    InfantryType();
  } else if (t == UnitTypeClass::AbsID) {
    UnitType();
  } else {
    eprintf("unknown TypeClass: {}", static_cast<int>(t));
  }
}
