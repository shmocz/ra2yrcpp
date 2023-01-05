#pragma once
#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/objects.hpp"
#include "ra2/type_classes.hpp"
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
static constexpr std::uintptr_t p_SHP_GetPixels = 0x69E740u;
static constexpr std::uintptr_t current_frame = 0xA8ED84u;
static constexpr std::uintptr_t p_SelectUnit = 0x6FBFA0u;
static constexpr std::uintptr_t p_SellObject = 0x4D9F70u;
static constexpr std::uintptr_t p_SellBuilding = 0x447110u;
static constexpr std::uintptr_t p_DeployObject = 0x7393C0u;
static constexpr std::uintptr_t p_CanBuildingBeSold = 0x4494C0u;
static constexpr std::uintptr_t p_ClickedEvent = 0x6ffe00;
constexpr unsigned MAX_PLAYERS = 8U;

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
  void add_AbstractTypeClass(std::unique_ptr<AbstractTypeClass> a,
                             const std::uintptr_t real_address);
  void add_HouseClass(std::unique_ptr<objects::HouseClass> h);
  void add_FactoryClass(std::unique_ptr<objects::FactoryClass> f);
  atc_map_t& abstract_type_classes();
  houseclass_vec_t& house_classes();
  factoryclass_vec_t& factory_classes();
  std::map<std::uintptr_t, std::unique_ptr<objects::ObjectClass>> objects;
  std::map<u32*, std::unique_ptr<type_classes::SHPStruct>> cameos;

 private:
  atc_map_t abstract_type_classes_;
  object_vec_t object_classes_;
  houseclass_vec_t house_classes_;
  factoryclass_vec_t factory_classes_;
};

}  // namespace game_state

}  // namespace ra2
