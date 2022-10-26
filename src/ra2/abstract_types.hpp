#pragma once
#include "ra2/general.h"

#include <string>

namespace ra2 {
namespace abstract_types {

namespace {
using namespace ra2::general;
}

struct AbstractClass {
  AbstractType type_id;
  AbstractFlags flags;
};

struct AbstractTypeClass : public AbstractClass {
  u32 p_vtable;
  std::string name;
};

}  // namespace abstract_types
}  // namespace ra2
