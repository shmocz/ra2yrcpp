#include "ra2/utility.hpp"

using namespace ra2::utility;

// TODO: make const
AbstractTypeEntry ra2::utility::get_AbstractType(const void* vtable_address) {
#define X(addr, name)                   \
  {                                     \
    addr, { AbstractType::name, #name } \
  }
  static const std::map<u32, AbstractTypeEntry> M = {
      X(0x7f5c70, Unit),          X(0x7f6218, UnitType),
      X(0x7e22a4, Aircraft),      X(0x7e2868, AircraftType),
      X(0x7eb058, Infantry),      X(0x7e3ebc, Building),
      X(0x7eb610, InfantryType),  X(0x7e4570, BuildingType),
      X(0x7ef600, OverlayType),   X(0x7f6b30, WarheadType),
      X(0x7f3528, SmudgeType),    X(0x7e2a50, AITriggerType),
      X(0x7e3608, AnimType),      X(0x7e4948, BulletType),
      X(0x7e4a28, Campaign),      X(0x7eab58, HouseType),
      X(0x7ecc48, IsotileType),   X(0x7f00a8, ParticleSystemType),
      X(0x7f0188, ParticleType),  X(0x7f1008, ScriptType),
      X(0x7f2ec0, Side),          X(0x7f4090, SuperWeaponType),
      X(0x7f45c4, TagType),       X(0x7f4680, TaskForce),
      X(0x7f47d0, TeamType),      X(0x7f5458, TerrainType),
      X(0x7f5728, Tiberium),      X(0x7f5904, TriggerType),
      X(0x7f6548, VoxelAnimType), X(0x7f73b8, WeaponType)};
  try {
    return M.at(reinterpret_cast<u32>(vtable_address));
  } catch (const std::out_of_range& e) {
    throw std::runtime_error(std::string("Could not map vtable ") +
                             yrclient::to_hex(vtable_address));
  }
#undef X
}
