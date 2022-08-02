#include "state_parser.hpp"

// TODO: put to abi
#include <xbyak/xbyak.h>

using namespace ra2::state_parser;
using namespace ra2::general;

// NOLINT
struct MemoryReader {
  explicit MemoryReader(void* base) : base(base) {}

  template <typename T>
  void read_item(T* dest, const std::size_t offset) {
    auto val = serialize::read_obj<T>(reinterpret_cast<u8*>(base) + offset);
    *dest = val;
  }
  void* base;
};

ra2::vectors::DynamicVectorClass<void*> ra2::state_parser::get_DVC(
    void* address) {
  ra2::vectors::DynamicVectorClass<void*> res;
  MemoryReader R(address);
  R.read_item(&res.Items, 1 * 0x4);
  R.read_item(&res.Capacity, 2 * 0x4);
  R.read_item(&res.IsInitialized, 3 * 0x4);
  R.read_item(&res.IsAllocated, 3 * 0x4 + 1);
  R.read_item(&res.Count, 4 * 0x4);
  R.read_item(&res.CapacityIncrement, 5 * 0x4);

  return res;
}

void ra2::state_parser::parse_AbstractClass(
    ra2::abstract_types::AbstractClass* dest, void* src) {
  MemoryReader R(src);
  R.read_item(&dest->type_id, 0x4);
  R.read_item(&dest->flags, 0x8);
}

void ra2::state_parser::parse_AbstractTypeClass(
    ra2::abstract_types::AbstractTypeClass* dest, void* address) {
  MemoryReader R(address);
  R.read_item(&dest->p_vtable, 0x0);
  char buf[0x31];
  std::memcpy(&buf[0], static_cast<char*>(address) + 0x64, sizeof(buf));
  dest->name = std::string(buf, sizeof(buf) - 1);
  dest->name = dest->name.substr(0, dest->name.find('\0'));
}

void ra2::state_parser::parse_BuildingTypeClass(
    ra2::type_classes::BuildingTypeClass* dest, void* address) {
  parse_TechnoTypeClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->foundation, 0xef0);
  R.read_item(&dest->p_foundation_data, 0xdfc);
}

// cppcheck-suppress unusedFunction
std::unique_ptr<ra2::type_classes::SHPStruct> load_SHPStruct(void* address) {
  std::unique_ptr<ra2::type_classes::SHPStruct> p;
  parse_SHPStruct(p.get(), address);
  return p;
}

void ra2::state_parser::parse_TechnoTypeClass(
    ra2::type_classes::TechnoTypeClass* dest, void* address) {
  parse_ObjectTypeClass(dest, address);
  MemoryReader R(address);
#define X(f, o) R.read_item(&dest->f, o)
  X(build_time_multiplier, 0x608);
  X(cost, 0x610 + 0 * 0x4);
  X(soylent, 0x610 + 1 * 0x4);
  X(flightlevel, 0x610 + 2 * 0x4);
  X(airstriketeam, 0x610 + 3 * 0x4);
  X(eliteairstriketeam, 0x610 + 4 * 0x4);
  X(airstrikerechargetime, 0x610 + 7 * 0x4);
  X(eliteairstrikerechargetime, 0x610 + 8 * 0x4);
  X(threatposed, 0x670 + 0 * 0x4);
  X(points, 0x670 + 1 * 0x4);
  X(speed, 0x670 + 2 * 0x4);
  X(speed_type, 0x670 + 3 * 0x4);
  X(p_cameo, 0x6f0);
#undef X
}

void ra2::state_parser::parse_ObjectTypeClass(
    ra2::type_classes::ObjectTypeClass* dest, void* address) {
  parse_AbstractTypeClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->armor, 0x9c);
  R.read_item(&dest->strength, 0xa0);
  dest->pointer_self = reinterpret_cast<u32>(address);
}

void ra2::state_parser::parse_InfantryTypeClass(
    ra2::type_classes::InfantryTypeClass* dest, void* address) {
  parse_TechnoTypeClass(dest, address);
}

void ra2::state_parser::parse_AircraftClass(ra2::objects::AircraftClass* dest,
                                            void* address) {
  parse_FootClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->aircraft_type, 0x6c4);
  R.read_item(&dest->p_type, 0x6c4);
}

void ra2::state_parser::parse_HouseClass(ra2::objects::HouseClass* dest,
                                         void* address) {
  MemoryReader R(address);
  R.read_item(&dest->start_credits, 0x1dc);
  R.read_item(&dest->current_player, 0x1ec);
  R.read_item(&dest->array_index, 0x30);
  R.read_item(&dest->house_type, 0x34);
  R.read_item(&dest->defeated, 0x1f5);
  R.read_item(&dest->money, 0x30c);
  R.read_item(&dest->is_game_over, 0x1f6);
  R.read_item(&dest->is_winner, 0x1f7);
  R.read_item(&dest->is_loser, 0x1f8);
  dest->self = reinterpret_cast<u32>(address);
  wchar_t buf[21];
  std::memcpy(&buf[0], static_cast<char*>(address) + 0x1602a, sizeof(buf));
  std::wstring w = buf;
  dest->name = std::string(w.begin(), w.end());
}

std::unique_ptr<ra2::objects::HouseClass>
ra2::state_parser::parse_HouseClassInstance(void* address) {
  auto p = std::make_unique<ra2::objects::HouseClass>();
  parse_HouseClass(p.get(), address);
  return p;
}

void ra2::state_parser::parse_AircraftTypeClass(
    ra2::type_classes::AircraftTypeClass* dest, void* address) {
  parse_TechnoTypeClass(dest, address);
}

void ra2::state_parser::parse_UnitTypeClass(
    ra2::type_classes::UnitTypeClass* dest, void* address) {
  parse_TechnoTypeClass(dest, address);
}

// FIXME: possible memleak if parsing fails. use uptr
ra2::abstract_types::AbstractTypeClass*
ra2::state_parser::parse_AbstractTypeClassInstance(void* address) {
  using ra2::abstract_types::AbstractTypeClass;
  using namespace ra2::type_classes;
  MemoryReader R(address);
  // Reader R(address);
  u32 p_vtable;
  try {
    R.read_item(&p_vtable, 0x0);
  } catch (const yrclient::system_error& e) {
    DPRINTF("ERROR: %p %s\n", address, e.what());
    return nullptr;
  }
  auto at = ra2::utility::get_AbstractType(reinterpret_cast<void*>(p_vtable));
#if 1
#define X(T)                      \
  case AbstractType::T: {         \
    auto* p = new T##Class();     \
    parse_##T##Class(p, address); \
    return p;                     \
  }
  switch (at.t) {
    case AbstractType::Abstract: {
      auto* p = new ra2::abstract_types::AbstractTypeClass();
      parse_AbstractClass(p, address);
      return p;
    }
      X(BuildingType);
      X(AircraftType);
      X(UnitType);
      X(InfantryType);
    default:
      break;
  }
#undef X
#endif
  fmt::print(stderr, "[ERROR]: failed to parse: {}\n", at.name);
  return nullptr;
}

using ra2::vectors::DynamicVectorClass;

template <typename T>
static void dvc_for_each(DynamicVectorClass<T>* D, std::function<void(T*)> fn) {
  for (int i = 0; i < D->Count; i++) {
    fn(reinterpret_cast<T>(D->Items) + i);
  }
}

void ra2::state_parser::parse_AbstractTypeClasses(ra2::game_state::GameState* G,
                                                  void* address) {
  auto DVC = get_DVC(reinterpret_cast<void*>(address));
  const auto count_init = DVC.Count;
  for (int i = 0; i < count_init; i++) {
    auto pu_obj =
        serialize::read_obj<u32>(reinterpret_cast<u32*>(DVC.Items) + i);
    u32* p_obj = reinterpret_cast<u32*>(pu_obj);
    auto ATC = std::unique_ptr<abstract_types::AbstractTypeClass>(
        parse_AbstractTypeClassInstance(p_obj));
    if (ATC != nullptr) {
      try {
        G->add_AbstractTypeClass(std::move(ATC), p_obj);
      } catch (const std::runtime_error& e) {
        fmt::print(stderr,
                   "not adding AbstractTypeClass with duplicate key {}\n",
                   reinterpret_cast<void*>(p_obj));
      }
    } else {
    }
  }
}

void ra2::state_parser::parse_ObjectClass(ra2::objects::ObjectClass* dest,
                                          void* address) {
  parse_AbstractClass(dest, address);
  MemoryReader R(address);
  static constexpr auto offset = 28 * 0x4;
  dest->id = reinterpret_cast<u32>(address);
  R.read_item(&dest->health, offset);
  R.read_item(&dest->coords, offset + 11 * 0x4);
}

void ra2::state_parser::parse_MissionClass(ra2::objects::MissionClass* dest,
                                           void* address) {
  parse_ObjectClass(dest, address);
}

void ra2::state_parser::parse_RadioClass(ra2::objects::RadioClass* dest,
                                         void* address) {
  parse_MissionClass(dest, address);
}

void ra2::state_parser::parse_TechnoClass(ra2::objects::TechnoClass* dest,
                                          void* address) {
  parse_RadioClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->owner, 0x21c);
  R.read_item(&dest->mind_controlled_by, 0x2c0);
  R.read_item(&dest->armor_multiplier, 0x158);
  R.read_item(&dest->firepower_multiplier, 0x160);
  R.read_item(&dest->shielded, 0x1d0);
  R.read_item(&dest->deactivated, 0x1d4);
}

void ra2::state_parser::parse_FootClass(ra2::objects::FootClass* dest,
                                        void* address) {
  parse_TechnoClass(dest, address);
  MemoryReader R(address);

  R.read_item(&dest->destination, 0x5a4);
  R.read_item(&dest->speed_percentage, 0x578 + 0 * 0x4);
  R.read_item(&dest->speed_multiplier, 0x578 + 1 * 0x4);
}

void ra2::state_parser::parse_UnitClass(ra2::objects::UnitClass* dest,
                                        void* address) {
  parse_FootClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->p_type, 0x6c4);
}

void ra2::state_parser::parse_BuildingClass(ra2::objects::BuildingClass* dest,
                                            void* address) {
  parse_TechnoClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->p_type, 0x520);
  R.read_item(&dest->build_state_type, 0x540);
  R.read_item(&dest->queue_build_state, 0x544);
}

void ra2::state_parser::parse_InfantryClass(ra2::objects::InfantryClass* dest,
                                            void* address) {
  parse_FootClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->p_type, 0x6c0);
}

template <typename T, typename ParseT>
static T* get_obj(void* address, T* O, ParseT fn) {
  if (O == nullptr) {
    O = new T();
  }
  fn(O, address);
  return O;
}

static ra2::objects::ObjectClass* parse_ObjectClassFromVtable(
    void* address, ra2::objects::ObjectClass* O) {
  MemoryReader R(address);
  u32 p_vtable;
  R.read_item(&p_vtable, 0x0);

  auto at = ra2::utility::get_AbstractType(reinterpret_cast<void*>(p_vtable));

  switch (at.t) {
    case AbstractType::Building:
      return get_obj(address, reinterpret_cast<ra2::objects::BuildingClass*>(O),
                     &ra2::state_parser::parse_BuildingClass);
    case AbstractType::Unit:
      return get_obj(address, reinterpret_cast<ra2::objects::UnitClass*>(O),
                     &ra2::state_parser::parse_UnitClass);
    case AbstractType::Infantry:
      return get_obj(address, reinterpret_cast<ra2::objects::InfantryClass*>(O),
                     &ra2::state_parser::parse_InfantryClass);
    case AbstractType::Aircraft:
      return get_obj(address, reinterpret_cast<ra2::objects::AircraftClass*>(O),
                     &ra2::state_parser::parse_AircraftClass);
    default:
      throw yrclient::general_error("Unknown AbstractType");
  }
}

void ra2::state_parser::parse_DVC_Objects(ra2::game_state::GameState* G,
                                          void* address) {
  auto DVC = get_DVC(address);

  // 1. for each DVC item
  // 2. if item not in GameState, allocate new object
  // 3. parse object
  // 4. remove objects that were not parsed
  static std::unordered_set<u32*> parsedd;
  parsedd.clear();
  for (const auto& [k, v] : G->objects) {
    parsedd.insert(k);
  }
  for (int i = 0; i < DVC.Count; i++) {
    auto p_obj =
        serialize::read_obj<u32>(reinterpret_cast<u32*>(DVC.Items) + i);
    u32* key = reinterpret_cast<u32*>(p_obj);
    auto it = G->objects.find(key);
    ra2::objects::ObjectClass* O = nullptr;
    try {
      if (it == G->objects.end()) {
        G->objects.try_emplace(
            key, std::unique_ptr<ra2::objects::ObjectClass>(
                     parse_ObjectClassFromVtable(reinterpret_cast<void*>(p_obj),
                                                 O)));
      } else {
        parse_ObjectClassFromVtable(reinterpret_cast<void*>(p_obj),
                                    it->second.get());
      }
      parsedd.erase(key);
    } catch (const yrclient::system_error& e) {
      fmt::print(stderr, "[ERROR]: %s\n", e.what());
    }
  }
  // remove non-parsed
  for (const auto& k : parsedd) {
    G->objects.erase(k);
  }
}

void ra2::state_parser::parse_DVC_HouseClasses(ra2::game_state::GameState* G,
                                               void* address) {
  auto DVC = get_DVC(reinterpret_cast<void*>(address));
  G->house_classes().clear();
  for (int i = 0; i < DVC.Count; i++) {
    auto p_obj =
        serialize::read_obj<u32>(reinterpret_cast<u32*>(DVC.Items) + i);
    auto H = parse_HouseClassInstance(reinterpret_cast<void*>(p_obj));

    if (H != nullptr) {
      try {
        G->add_HouseClass(std::move(H));
      } catch (const std::runtime_error& e) {
        DPRINTF("fail!\n");
      }
    } else {
    }
  }
}

std::unique_ptr<ra2::objects::FactoryClass>
ra2::state_parser::parse_FactoryClassInstance(void* address) {
  auto p = std::make_unique<ra2::objects::FactoryClass>();
  parse_FactoryClass(p.get(), address);
  return p;
}

void ra2::state_parser::parse_DVC_FactoryClasses(ra2::game_state::GameState* G,
                                                 void* address) {
  auto DVC = get_DVC(address);
  G->factory_classes().clear();
  for (int i = 0; i < DVC.Count; i++) {
    auto p_obj =
        serialize::read_obj<u32>(reinterpret_cast<u32*>(DVC.Items) + i);
    auto H = parse_FactoryClassInstance(reinterpret_cast<void*>(p_obj));

    if (H != nullptr) {
      try {
        G->add_FactoryClass(std::move(H));
      } catch (const std::runtime_error& e) {
        DPRINTF("fail!\n");
      }
    }
  }
}

void ra2::state_parser::parse_ProgressTimer(ra2::objects::ProgressTimer* dest,
                                            void* address) {
  MemoryReader R(address);
  R.read_item(&dest->value, 0x0);
}

// TODO: ensure we do correct pointer arithmetic everywhere
void ra2::state_parser::parse_FactoryClass(ra2::objects::FactoryClass* dest,
                                           void* address) {
  parse_AbstractClass(dest, address);
  MemoryReader R(address);
  get_DVC(reinterpret_cast<u8*>(address) + 0x80);
  R.read_item(&dest->owner, 0x6c);
  R.read_item(&dest->object, 0x58);
  parse_ProgressTimer(&dest->production,
                      reinterpret_cast<u8*>(address) + offset_AbstractClass);
}

struct GetSHPPixelData : Xbyak::CodeGenerator {
  GetSHPPixelData() {
    mov(ecx, ptr[esp + 0x4]);
    mov(eax, ptr[esp + 0x8]);  // object index
    push(eax);
    mov(eax, 0x69E740u);
    call(eax);
    ret();
  }
};

void ra2::state_parser::parse_SHPStruct(ra2::type_classes::SHPStruct* dest,
                                        void* address) {
  MemoryReader R(address);
  R.read_item(&dest->width, 0x2 + 0 * 2);
  R.read_item(&dest->height, 0x2 + 1 * 2);
  R.read_item(&dest->frames, 0x2 + 2 * 2);
  typedef u8* __cdecl (*fn_SHP_GetPixels_t)(int, int);
  static GetSHPPixelData C;
  auto* fn = C.getCode<fn_SHP_GetPixels_t>();
  for (int i = 0; i < static_cast<int>(dest->frames); i++) {
    auto* buf = fn(reinterpret_cast<u32>(address), i);
    if (buf != nullptr) {
      dest->pixel_data.emplace_back(buf, buf + dest->width * dest->height);
    }
  }
}

void ra2::state_parser::parse_cameos(ra2::game_state::GameState* G) {
  for (const auto& [k, v] : G->abstract_type_classes()) {
    if (utility::is_technotypeclass(reinterpret_cast<void*>(v->p_vtable))) {
      auto* ttc =
          reinterpret_cast<ra2::type_classes::TechnoTypeClass*>(v.get());
      auto* key = reinterpret_cast<u32*>(ttc->p_cameo);
      if (key != nullptr) {
        if (G->cameos.find(key) == G->cameos.end()) {
          auto* SHP = new type_classes::SHPStruct();
          parse_SHPStruct(SHP, key);
          G->cameos.try_emplace(key, SHP);
        }
      }
    }
  }
}
