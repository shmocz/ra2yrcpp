#pragma once
#include "debug_helpers.h"
#include "errors.hpp"
#include "ra2/abstract_types.hpp"
#include "ra2/game_state.hpp"
#include "ra2/general.h"
#include "ra2/objects.hpp"
#include "ra2/type_classes.hpp"
#include "ra2/utility.hpp"
#include "ra2/vectors.hpp"
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

// TODO: use this everywhere
constexpr std::ptrdiff_t offset_AbstractClass = 0x24;

ra2::vectors::DynamicVectorClass<void*> get_DVC(void* address);
void parse_AbstractClass(ra2::abstract_types::AbstractClass* dest, void* src);
void parse_AbstractTypeClasses(ra2::game_state::GameState* G, void* address);

ra2::abstract_types::AbstractTypeClass* parse_AbstractTypeClassInstance(
    void* address);
void parse_AbstractTypeClass(ra2::abstract_types::AbstractTypeClass* dest,
                             void* address);

void parse_BuildingTypeClass(ra2::type_classes::BuildingTypeClass* dest,
                             void* address);

void parse_TechnoTypeClass(ra2::type_classes::TechnoTypeClass* dest,
                           void* address);

void parse_ObjectTypeClass(ra2::type_classes::ObjectTypeClass* dest,
                           void* address);

void parse_InfantryTypeClass(ra2::type_classes::InfantryTypeClass* dest,
                             void* address);

void parse_AircraftClass(ra2::objects::AircraftClass* dest, void* address);

void parse_AircraftTypeClass(ra2::type_classes::AircraftTypeClass* dest,
                             void* address);

void parse_UnitTypeClass(ra2::type_classes::UnitTypeClass* dest, void* address);

void parse_ObjectClass(ra2::objects::ObjectClass* dest, void* address);

void parse_MissionClass(ra2::objects::MissionClass* dest, void* address);

void parse_RadioClass(ra2::objects::RadioClass* dest, void* address);

void parse_TechnoClass(ra2::objects::TechnoClass* dest, void* address);

void parse_FootClass(ra2::objects::FootClass* dest, void* address);

void parse_UnitClass(ra2::objects::UnitClass* dest, void* address);

void parse_BuildingClass(ra2::objects::BuildingClass* dest, void* address);

void parse_InfantryClass(ra2::objects::InfantryClass* dest, void* address);

void parse_HouseClass(ra2::objects::HouseClass* dest, void* address);

std::unique_ptr<ra2::objects::HouseClass> parse_HouseClassInstance(
    void* address);

void parse_DVC_Objects(ra2::game_state::GameState* G, void* address);

void parse_DVC_HouseClasses(ra2::game_state::GameState* G, void* address);

void parse_ProgressTimer(ra2::objects::ProgressTimer* dest, void* address);

void parse_FactoryClass(ra2::objects::FactoryClass* dest, void* address);

std::unique_ptr<ra2::objects::FactoryClass> parse_FactoryClassInstance(
    void* address);

void parse_DVC_FactoryClasses(ra2::game_state::GameState* G, void* address);

void parse_SHPStruct(ra2::type_classes::SHPStruct* dest, void* address);

void parse_cameos(ra2::game_state::GameState* G);

}  // namespace state_parser
}  // namespace ra2
