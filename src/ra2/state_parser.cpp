#include "state_parser.hpp"

// TODO: put to abi
#include <xbyak/xbyak.h>

using namespace ra2::state_parser;
using namespace ra2::general;

// NOLINT
struct MemoryReader {
  explicit MemoryReader(std::uintptr_t base) : base(base) {}

  template <typename T>
  void read_item(T* dest, const std::size_t offset) {
    auto val = serialize::read_obj<T>(base + offset);
    *dest = val;
  }

  std::uintptr_t base;
};

ra2::vectors::DynamicVectorClass<void*> ra2::state_parser::get_DVC(
    std::uintptr_t address) {
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
    ra2::abstract_types::AbstractClass* dest, std::uintptr_t src) {
  MemoryReader R(src);
  R.read_item(&dest->type_id, 0x4);
  R.read_item(&dest->flags, 0x8);
}

void ra2::state_parser::parse_AbstractTypeClass(
    ra2::abstract_types::AbstractTypeClass* dest, std::uintptr_t address) {
  MemoryReader R(address);
  R.read_item(&dest->p_vtable, 0x0);
  char buf[0x31];
  std::memcpy(&buf[0], ::utility::asptr<char*>(address + 0x64), sizeof(buf));
  dest->name = std::string(buf, sizeof(buf) - 1);
  dest->name = dest->name.substr(0, dest->name.find('\0'));
}

void ra2::state_parser::parse_BuildingTypeClass(
    ra2::type_classes::BuildingTypeClass* dest, std::uintptr_t address) {
  parse_TechnoTypeClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->foundation, 0xef0);
  R.read_item(&dest->p_foundation_data, 0xdfc);
}

// cppcheck-suppress unusedFunction
std::unique_ptr<ra2::type_classes::SHPStruct> load_SHPStruct(
    std::uintptr_t address) {
  std::unique_ptr<ra2::type_classes::SHPStruct> p;
  parse_SHPStruct(p.get(), address);
  return p;
}

void ra2::state_parser::parse_TechnoTypeClass(
    ra2::type_classes::TechnoTypeClass* dest, std::uintptr_t address) {
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
    ra2::type_classes::ObjectTypeClass* dest, std::uintptr_t address) {
  parse_AbstractTypeClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->armor, 0x9c);
  R.read_item(&dest->strength, 0xa0);
  dest->pointer_self = address;
}

void ra2::state_parser::parse_InfantryTypeClass(
    ra2::type_classes::InfantryTypeClass* dest, std::uintptr_t address) {
  parse_TechnoTypeClass(dest, address);
}

void ra2::state_parser::parse_AircraftClass(ra2::objects::AircraftClass* dest,
                                            std::uintptr_t address) {
  parse_FootClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->aircraft_type, 0x6c4);
  R.read_item(&dest->p_type, 0x6c4);
}

void ra2::state_parser::parse_HouseClass(ra2::objects::HouseClass* dest,
                                         std::uintptr_t address) {
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
  R.read_item(&dest->power_output, 0x53a4);
  R.read_item(&dest->power_drain, 0x53a8);
  dest->self = address;
  wchar_t buf[21];
  std::memcpy(&buf[0], ::utility::asptr<char*>(address + 0x1602a), sizeof(buf));
  std::wstring w = buf;
  dest->name = std::string(w.begin(), w.end());
}

std::unique_ptr<ra2::objects::HouseClass>
ra2::state_parser::parse_HouseClassInstance(std::uintptr_t address) {
  auto p = std::make_unique<ra2::objects::HouseClass>();
  parse_HouseClass(p.get(), address);
  return p;
}

void ra2::state_parser::parse_AircraftTypeClass(
    ra2::type_classes::AircraftTypeClass* dest, std::uintptr_t address) {
  parse_TechnoTypeClass(dest, address);
}

void ra2::state_parser::parse_UnitTypeClass(
    ra2::type_classes::UnitTypeClass* dest, std::uintptr_t address) {
  parse_TechnoTypeClass(dest, address);
}

std::unique_ptr<ra2::abstract_types::AbstractTypeClass>
ra2::state_parser::parse_AbstractTypeClassInstance(
    const std::uintptr_t address) {
  using ra2::abstract_types::AbstractTypeClass;
  using namespace ra2::type_classes;
  MemoryReader R(address);
  u32 p_vtable = 0U;
  try {
    R.read_item(&p_vtable, 0x0);
  } catch (const yrclient::system_error& e) {
    eprintf("{} {}", address, e.what());
    return nullptr;
  }
  auto at = ra2::utility::get_AbstractType(::utility::asptr(p_vtable));
#define X(T)                               \
  case AbstractType::T: {                  \
    auto q = std::make_unique<T##Class>(); \
    parse_##T##Class(q.get(), address);    \
    return q;                              \
  }
  switch (at.t) {
    case AbstractType::Abstract: {
      auto q = std::make_unique<ra2::abstract_types::AbstractTypeClass>();
      parse_AbstractClass(q.get(), address);
      return q;
    }
      X(BuildingType);
      X(AircraftType);
      X(UnitType);
      X(InfantryType);
    default:
      break;
  }
#undef X
  eprintf("failed to parse: {}", at.name);
  return nullptr;
}

void ra2::state_parser::parse_AbstractTypeClasses(ra2::game_state::GameState* G,
                                                  std::uintptr_t address) {
  auto DVC = get_DVC(address);
  const auto count_init = DVC.Count;
  for (int i = 0; i < count_init; i++) {
    auto pu_obj =
        serialize::read_obj<u32>(::utility::asint(DVC.Items) + i * 0x4);
    auto ATC = parse_AbstractTypeClassInstance(pu_obj);
    // TODO(shmocz): just throw?
    if (ATC != nullptr) {
      try {
        G->add_AbstractTypeClass(std::move(ATC), pu_obj);
      } catch (const std::runtime_error& e) {
        eprintf("not adding AbstractTypeClass with duplicate key {}",
                ::utility::asptr(pu_obj));
      } catch (...) {
        eprintf("fatal error");
      }
    } else {
    }
  }
}

void ra2::state_parser::parse_ObjectClass(ra2::objects::ObjectClass* dest,
                                          std::uintptr_t address) {
  parse_AbstractClass(dest, address);
  MemoryReader R(address);
  static constexpr auto offset = 28 * 0x4;
  dest->id = address;
  R.read_item(&dest->health, offset);
  R.read_item(&dest->coords, offset + 11 * 0x4);
}

void ra2::state_parser::parse_MissionClass(ra2::objects::MissionClass* dest,
                                           std::uintptr_t address) {
  parse_ObjectClass(dest, address);
}

void ra2::state_parser::parse_RadioClass(ra2::objects::RadioClass* dest,
                                         std::uintptr_t address) {
  parse_MissionClass(dest, address);
}

void ra2::state_parser::parse_TechnoClass(ra2::objects::TechnoClass* dest,
                                          std::uintptr_t address) {
  parse_RadioClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->owner, 0x21c);
  R.read_item(&dest->mind_controlled_by, 0x2c0);
  R.read_item(&dest->originally_owned_by, 0x14c);
  R.read_item(&dest->armor_multiplier, 0x158);
  R.read_item(&dest->firepower_multiplier, 0x160);
  R.read_item(&dest->shielded, 0x1d0);
  R.read_item(&dest->deactivated, 0x1d4);
}

void ra2::state_parser::parse_FootClass(ra2::objects::FootClass* dest,
                                        std::uintptr_t address) {
  parse_TechnoClass(dest, address);
  MemoryReader R(address);

  R.read_item(&dest->destination, 0x5a4);
  R.read_item(&dest->speed_percentage, 0x578 + 0 * 0x4);
  R.read_item(&dest->speed_multiplier, 0x578 + 1 * 0x4);
}

void ra2::state_parser::parse_UnitClass(ra2::objects::UnitClass* dest,
                                        std::uintptr_t address) {
  parse_FootClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->p_type, 0x6c4);
}

void ra2::state_parser::parse_BuildingClass(ra2::objects::BuildingClass* dest,
                                            std::uintptr_t address) {
  parse_TechnoClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->p_type, 0x520);
  R.read_item(&dest->build_state_type, 0x540);
  R.read_item(&dest->queue_build_state, 0x544);
  R.read_item(&dest->owner_country_index, 0x548);
}

void ra2::state_parser::parse_InfantryClass(ra2::objects::InfantryClass* dest,
                                            std::uintptr_t address) {
  parse_FootClass(dest, address);
  MemoryReader R(address);
  R.read_item(&dest->p_type, 0x6c0);
}

struct Entry {
  AbstractType t;
  std::function<std::unique_ptr<ra2::objects::ObjectClass>(void)> fn_create;
  std::function<void(ra2::objects::ObjectClass*, std::uintptr_t address)>
      fn_parse;
};

template <typename T, typename FnT>
auto get_entry(FnT f) {
  return Entry{T::atype,
               []() {
                 return std::unique_ptr<ra2::objects::ObjectClass>(
                     std::make_unique<T>());
               },
               [f](ra2::objects::ObjectClass* p, std::uintptr_t address) {
                 f(static_cast<T*>(p), address);
               }};
}

auto* get_lut() {
  static const std::array<Entry, 4> lut = {
      {get_entry<ra2::objects::BuildingClass>(
           &ra2::state_parser::parse_BuildingClass),
       get_entry<ra2::objects::InfantryClass>(
           &ra2::state_parser::parse_InfantryClass),
       get_entry<ra2::objects::UnitClass>(&ra2::state_parser::parse_UnitClass),
       get_entry<ra2::objects::AircraftClass>(
           &ra2::state_parser::parse_AircraftClass)}};
  return &lut;
}

void get_object(std::unique_ptr<ra2::objects::ObjectClass>& o,  // NOLINT
                std::uintptr_t address) {
  MemoryReader R(address);
  u32 p_vtable = 0U;
  R.read_item(&p_vtable, 0x0);
  auto at = ra2::utility::get_AbstractType(::utility::asptr<void*>(p_vtable));
  const auto* lut = get_lut();
  const auto* const f = std::find_if(
      lut->begin(), lut->end(), [&at](const Entry& a) { return a.t == at.t; });
  if (o == nullptr) {
    o = f->fn_create();
  }
  f->fn_parse(o.get(), address);
}

void ra2::state_parser::parse_DVC_Objects(ra2::game_state::GameState* G,
                                          std::uintptr_t address) {
  auto DVC = get_DVC(address);

  // 1. for each DVC item
  // 2. if item not in GameState, allocate new object
  // 3. parse object
  // 4. remove objects that were not parsed
  std::unordered_set<decltype(G->objects)::key_type> parsedd;
  for (const auto& [k, v] : G->objects) {
    parsedd.insert(k);
  }
  for (int i = 0; i < DVC.Count; i++) {
    const auto p_obj =
        serialize::read_obj<u32>(::utility::asint(DVC.Items) + i * 0x4);
    auto it = G->objects.find(p_obj);
    try {
      if (it == G->objects.end()) {  // new object
        std::unique_ptr<ra2::objects::ObjectClass> obj_instance;
        get_object(obj_instance, p_obj);
        G->objects.try_emplace(p_obj, std::move(obj_instance));
      } else {  // existing object
        get_object(it->second, p_obj);
      }
      parsedd.erase(p_obj);
    } catch (const std::exception& e) {  // FIXME: potential memory leak
      eprintf("DVC objects failed {}", e.what());
    }
  }
  // remove non-parsed
  for (const auto& k : parsedd) {
    G->objects.erase(k);
  }
}

// FIXME: dont allocate every time
void ra2::state_parser::parse_DVC_HouseClasses(ra2::game_state::GameState* G,
                                               std::uintptr_t address) {
  auto DVC = get_DVC(address);
  G->house_classes().clear();
  for (int i = 0; i < DVC.Count; i++) {
    auto H = parse_HouseClassInstance(
        serialize::read_obj<u32>(::utility::asint(DVC.Items) + i * 0x4));

    if (H != nullptr) {
      try {
        G->add_HouseClass(std::move(H));
      } catch (const std::exception& e) {
        eprintf("couldn't add HouseClass");
      }
    } else {
      eprintf("null houseptr");
    }
  }
}

std::unique_ptr<ra2::objects::FactoryClass>
ra2::state_parser::parse_FactoryClassInstance(std::uintptr_t address) {
  auto p = std::make_unique<ra2::objects::FactoryClass>();
  parse_FactoryClass(p.get(), address);
  return p;
}

void ra2::state_parser::parse_DVC_FactoryClasses(ra2::game_state::GameState* G,
                                                 std::uintptr_t address) {
  auto DVC = get_DVC(address);
  G->factory_classes().clear();
  for (int i = 0; i < DVC.Count; i++) {
    auto H = parse_FactoryClassInstance(
        serialize::read_obj<u32>(::utility::asint(DVC.Items) + i * 0x4));

    if (H != nullptr) {
      try {
        G->add_FactoryClass(std::move(H));
      } catch (const std::runtime_error& e) {
        eprintf("couldn't add FactoryClass");
      }
    }
  }
}

void ra2::state_parser::parse_ProgressTimer(ra2::objects::ProgressTimer* dest,
                                            std::uintptr_t address) {
  MemoryReader R(address);
  R.read_item(&dest->value, 0x0);
}

// TODO(shmocz): ensure we do correct pointer arithmetic everywhere
void ra2::state_parser::parse_FactoryClass(ra2::objects::FactoryClass* dest,
                                           std::uintptr_t address) {
  parse_AbstractClass(dest, address);
  MemoryReader R(address);
  get_DVC(address + 0x80);
  R.read_item(&dest->owner, 0x6c);
  R.read_item(&dest->object, 0x58);
  parse_ProgressTimer(&dest->production, address + offset_AbstractClass);
}

struct GetSHPPixelData : Xbyak::CodeGenerator {
  GetSHPPixelData() {
    mov(ecx, ptr[esp + 0x4]);
    mov(eax, ptr[esp + 0x8]);  // object index
    push(eax);
    mov(eax, 0x69E740U);
    call(eax);
    ret();
  }
};

void ra2::state_parser::parse_SHPStruct(ra2::type_classes::SHPStruct* dest,
                                        std::uintptr_t address) {
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
          parse_SHPStruct(SHP, reinterpret_cast<std::uintptr_t>(key));
          G->cameos.try_emplace(key, SHP);
        }
      }
    }
  }
}
