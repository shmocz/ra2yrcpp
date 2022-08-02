#pragma once
#include "ra2/general.h"
#include "util_string.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <string>

namespace ra2 {
namespace utility {

namespace {
using namespace ra2::general;
}

struct AbstractTypeEntry {
  AbstractType t;
  std::string name;
  friend std::ostream& operator<<(std::ostream& os, const AbstractTypeEntry& o);
};

std::ostream& operator<<(std::ostream& os, const AbstractTypeEntry& o);

AbstractTypeEntry get_AbstractType(const void* vtable_address);

inline bool is_technotypeclass(void* ptr) {
  auto at = ra2::utility::get_AbstractType(ptr);
  constexpr std::array<AbstractType, 4> A = {
      AbstractType::AircraftType, AbstractType::BuildingType,
      AbstractType::UnitType, AbstractType::InfantryType};
  return std::find(A.begin(), A.end(), at.t) != A.end();
}

}  // namespace utility
}  // namespace ra2
