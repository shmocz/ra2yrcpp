#pragma once

#include "protocol/protocol.hpp"

#include "errors.hpp"
#include "logging.hpp"
#include "ra2/abstract_types.hpp"
#include "ra2/event.hpp"
#include "ra2/game_state.hpp"
#include "ra2/general.h"
#include "ra2/objects.hpp"
#include "ra2/type_classes.hpp"
#include "ra2/utility.hpp"
#include "ra2/vectors.hpp"
#include "utility/memtools.hpp"
#include "utility/serialize.hpp"

#include <fmt/core.h>

#include <cstring>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace ra2 {
namespace state_parser {

// TODO(shmocz): use uintptr_t for pointer types whenever possible

// TODO: use this everywhere
constexpr std::ptrdiff_t offset_AbstractClass = 0x24;

ra2::vectors::DynamicVectorClass<void*> get_DVC(std::uintptr_t address);
void parse_AbstractClass(ra2::abstract_types::AbstractClass* dest,
                         std::uintptr_t src);
void parse_AbstractTypeClasses(ra2::game_state::GameState* G,
                               std::uintptr_t address);

std::unique_ptr<ra2::abstract_types::AbstractTypeClass>
parse_AbstractTypeClassInstance(const std::uintptr_t address);

void parse_AbstractTypeClass(ra2::abstract_types::AbstractTypeClass* dest,
                             std::uintptr_t address);

void parse_BuildingTypeClass(ra2::type_classes::BuildingTypeClass* dest,
                             std::uintptr_t address);

void parse_TechnoTypeClass(ra2::type_classes::TechnoTypeClass* dest,
                           std::uintptr_t address);

void parse_ObjectTypeClass(ra2::type_classes::ObjectTypeClass* dest,
                           std::uintptr_t address);

void parse_InfantryTypeClass(ra2::type_classes::InfantryTypeClass* dest,
                             std::uintptr_t address);

void parse_AircraftClass(ra2::objects::AircraftClass* dest,
                         std::uintptr_t address);

void parse_AircraftTypeClass(ra2::type_classes::AircraftTypeClass* dest,
                             std::uintptr_t address);

void parse_UnitTypeClass(ra2::type_classes::UnitTypeClass* dest,
                         std::uintptr_t address);

void parse_ObjectClass(ra2::objects::ObjectClass* dest, std::uintptr_t address);

void parse_MissionClass(ra2::objects::MissionClass* dest,
                        std::uintptr_t address);

void parse_RadioClass(ra2::objects::RadioClass* dest, std::uintptr_t address);

void parse_TechnoClass(ra2::objects::TechnoClass* dest, std::uintptr_t address);

void parse_FootClass(ra2::objects::FootClass* dest, std::uintptr_t address);

void parse_UnitClass(ra2::objects::UnitClass* dest, std::uintptr_t address);

void parse_BuildingClass(ra2::objects::BuildingClass* dest,
                         std::uintptr_t address);

void parse_InfantryClass(ra2::objects::InfantryClass* dest,
                         std::uintptr_t address);

void parse_HouseClass(ra2::objects::HouseClass* dest, std::uintptr_t address);

std::unique_ptr<ra2::objects::HouseClass> parse_HouseClassInstance(
    std::uintptr_t address);

void parse_DVC_Objects(ra2::game_state::GameState* G, std::uintptr_t address);

void parse_DVC_HouseClasses(ra2::game_state::GameState* G,
                            std::uintptr_t address);

void parse_ProgressTimer(ra2::objects::ProgressTimer* dest,
                         std::uintptr_t address);

void parse_FactoryClass(ra2::objects::FactoryClass* dest,
                        std::uintptr_t address);

std::unique_ptr<ra2::objects::FactoryClass> parse_FactoryClassInstance(
    std::uintptr_t address);

void parse_DVC_FactoryClasses(ra2::game_state::GameState* G,
                              std::uintptr_t address);

void parse_SHPStruct(ra2::type_classes::SHPStruct* dest,
                     std::uintptr_t address);

void parse_cameos(ra2::game_state::GameState* G);

template <typename EventT>
void parse_event_list(
    google::protobuf::RepeatedPtrField<ra2yrproto::ra2yr::Event>* dst) {
  auto* L = ::utility::asptr<EventT*>(EventT::address);
  dprintf("head={},tail={},size={}", L->Head, L->Tail, L->Count);
  for (auto i = L->Tail; i > 1; i--) {
    auto* e = dst->Add();
    e->set_timing(L->Timings[i - 1]);
  }
}

}  // namespace state_parser
}  // namespace ra2
