#pragma once
#include "ra2/general.h"
#include "util_string.hpp"

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

AbstractTypeEntry get_AbstractType(void* vtable_address);

}  // namespace utility
}  // namespace ra2
