#pragma once
#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/objects.hpp"
#include "ra2/vectors.hpp"
#include "util_string.hpp"
#include "utility.h"

#include <cstdint>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ra2 {
namespace game_state {

namespace {
using namespace ra2::general;
using namespace ra2::abstract_types;
}  // namespace

static constexpr std::uintptr_t p_DVC_AbstractTypeClasses = 0xA8E968u;
static constexpr std::uintptr_t p_DVC_CurrentObjects = 0xA8ECB8u;
static constexpr std::uintptr_t p_DVC_TechnoClasses = 0xA8EC78u;
static constexpr std::uintptr_t p_DVC_HouseClasses = 0xA80228u;
static constexpr std::uintptr_t p_DVC_FactoryClasses = 0xA83E30u;

struct RectangleStruct {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
};

struct TacticalClass : public ra2::abstract_types::AbstractClass {
  ra2::vectors::Matrix3D transform;
  virtual AbstractType id() const;
};

using atc_map_t = std::map<u32, std::unique_ptr<AbstractTypeClass>>;
using object_vec_t = std::vector<std::unique_ptr<objects::ObjectClass>>;
using houseclass_vec_t = std::vector<std::unique_ptr<objects::HouseClass>>;
using factoryclass_vec_t = std::vector<std::unique_ptr<objects::FactoryClass>>;

class GameState {
 public:
#if 0
  void add_ATC(const u32 p_obj, AbstractTypeClass2* A) {
    assert(!contains(m_ATCs, p_obj));
    m_ATCs[p_obj] = A;
    if (A != nullptr) {
      DPRINTF("Added ATC %s\n", A->name.c_str());
    }
  }
#endif

  void add_AbstractTypeClass(std::unique_ptr<AbstractTypeClass> a,
                             const std::uintptr_t* real_address);
  void add_ObjectClass(std::unique_ptr<objects::ObjectClass> a);
  void add_HouseClass(std::unique_ptr<objects::HouseClass> h);
  void add_FactoryClass(std::unique_ptr<objects::FactoryClass> f);
  atc_map_t& abstract_type_classes();
  object_vec_t& object_classes();
  houseclass_vec_t& house_classes();
  factoryclass_vec_t& factory_classes();
  std::map<u32*, std::unique_ptr<objects::ObjectClass>> objects;

 private:
  // vectors::DynamicVectorClass<void*> abstract_type_classes_;
  atc_map_t abstract_type_classes_;
  object_vec_t object_classes_;
  houseclass_vec_t house_classes_;
  factoryclass_vec_t factory_classes_;
};

#if 0

  m_Tactical = new TacticalClass(m.set_pos(paddr(0x887324u)));
void GlobalData::read_common() {
  read_tactical();
  read_bounds();
  read_AbstractClasses();
  read_AbstractTypeClasses2();
  read_TechnoClasses();
}

#endif
}  // namespace game_state

}  // namespace ra2
