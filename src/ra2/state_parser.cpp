#include "ra2/state_parser.hpp"

#include "ra2yrproto/ra2yr.pb.h"

#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "ra2/abi.hpp"
#include "ra2/yrpp_export.hpp"
#include "utility/array_iterator.hpp"

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>

using namespace ra2;

ClassParser::ClassParser(Cookie c, ra2yrproto::ra2yr::Object* T) : c(c), T(T) {}

void Cell::copy_to(ra2yrproto::ra2yr::Cell* dst, const Cell* src) {
  dst->set_land_type(static_cast<ra2yrproto::ra2yr::LandType>(src->land_type));
  dst->set_radiation_level(src->radiation_level);
  dst->set_height(static_cast<i32>(src->height));
  dst->set_level(static_cast<i32>(src->level));
  dst->set_overlay_data(src->overlay_data);
  dst->set_tiberium_value(src->tiberium_value);
  dst->set_shrouded(src->shrouded);
  dst->set_passability(src->passability);
  dst->set_index(src->index);
}

void ClassParser::Object() {
  auto* P = reinterpret_cast<ObjectClass*>(c.src);
  T->set_health(P->Health);
  T->set_selected(P->IsSelected);
  T->set_in_limbo(P->InLimbo);
}

void ClassParser::Mission() {
  Object();

  auto* P = reinterpret_cast<MissionClass*>(c.src);
  T->set_current_mission(
      static_cast<ra2yrproto::ra2yr::Mission>(P->CurrentMission));
}

void ClassParser::Radio() { Mission(); }

void ClassParser::Techno() {
  Radio();
  auto* P = reinterpret_cast<TechnoClass*>(c.src);
  T->set_pointer_house(reinterpret_cast<u32>(P->Owner));
  T->set_pointer_initial_owner(reinterpret_cast<u32>(P->InitialOwner));
  // TODO: armor multiplier
  if (P->IsOnMap) {
    auto* q = T->mutable_coordinates();
    auto L = P->Location;
    q->set_x(L.X);
    q->set_y(L.Y);
    q->set_z(L.Z);
  }
}

void ClassParser::Foot() {
  Techno();
  auto* P = reinterpret_cast<FootClass*>(c.src);
  if (P->Destination != nullptr) {
    auto t = ra2::abi::AbstractClass_WhatAmI::call(c.abi, P->Destination);

    if (t == CellClass::AbsID) {
      auto* dest = reinterpret_cast<CellClass*>(P->Destination);
      auto coord = dest->Cell2Coord(dest->MapCoords);
      auto dd = T->mutable_destination();
      dd->set_x(coord.X);
      dd->set_y(coord.Y);
      dd->set_z(coord.Z);
    }
  }
}

void ClassParser::Aircraft() {
  Foot();
  auto* P = reinterpret_cast<AircraftClass*>(c.src);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_AIRCRAFT);
}

void ClassParser::Unit() {
  Foot();
  auto* P = reinterpret_cast<UnitClass*>(c.src);
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_UNIT);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
  T->set_deployed(P->Deployed);
  T->set_deploying(P->IsDeploying);
}

void ClassParser::Building() {
  Techno();
  auto* P = reinterpret_cast<BuildingClass*>(c.src);
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_BUILDING);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
}

void ClassParser::Infantry() {
  Foot();
  auto* P = reinterpret_cast<InfantryClass*>(c.src);
  T->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_INFANTRY);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(P->Type));
}

void ClassParser::parse() {
  T->set_pointer_self(reinterpret_cast<u32>(c.src));
  auto t = ra2::abi::AbstractClass_WhatAmI::call(
      c.abi, reinterpret_cast<AbstractClass*>(c.src));

  if (t == UnitClass::AbsID) {
    Unit();
  } else if (t == BuildingClass::AbsID) {
    Building();
  } else if (t == InfantryClass::AbsID) {
    Infantry();
  } else if (t == AircraftClass::AbsID) {
    Aircraft();
  } else {
    eprintf("unknown ObjectClass: {}", static_cast<int>(t));
  }
}

TypeClassParser::TypeClassParser(Cookie c,
                                 ra2yrproto::ra2yr::ObjectTypeClass* T)
    : c(c), T(T) {}

void TypeClassParser::AbstractType() {
  auto* P = reinterpret_cast<AbstractTypeClass*>(c.src);
  T->set_name(P->Name);
}

void TypeClassParser::AircraftType() {
  TechnoType();
  auto* P = reinterpret_cast<AircraftTypeClass*>(c.src);

  T->set_array_index(P->ArrayIndex);
  T->set_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_AIRCRAFTTYPE);
}

void TypeClassParser::TechnoType() {
  ObjectType();
  auto* P = reinterpret_cast<TechnoTypeClass*>(c.src);
  T->set_cost(P->Cost);
  T->set_soylent(P->Soylent);
  auto A = abi::DVCIterator(&P->Prerequisite);
  T->mutable_prerequisites()->Clear();
  for (auto r : A) {
    T->add_prerequisites(static_cast<int>(r));
  }
  T->set_required_houses(P->RequiredHouses);
  T->set_forbidden_houses(P->ForbiddenHouses);
  T->set_owner_flags(P->OwnerFlags);
  T->set_tech_level(P->TechLevel);
  T->set_build_limit(P->BuildLimit);
  T->set_naval(P->Naval);
  T->set_requires_stolen_allied_tech(P->RequiresStolenAlliedTech);
  T->set_requires_stolen_soviet_tech(P->RequiresStolenSovietTech);
  T->set_requires_stolen_third_tech(P->RequiresStolenThirdTech);
}

void TypeClassParser::UnitType() {
  TechnoType();
  auto* P = reinterpret_cast<UnitTypeClass*>(c.src);

  T->set_array_index(P->ArrayIndex);
  T->set_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_UNITTYPE);
}

void TypeClassParser::InfantryType() {
  TechnoType();
  auto* P = reinterpret_cast<InfantryTypeClass*>(c.src);

  T->set_array_index(P->ArrayIndex);
  T->set_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_INFANTRYTYPE);
}

void TypeClassParser::ObjectType() {
  AbstractType();
  auto* P = reinterpret_cast<ObjectTypeClass*>(c.src);
  T->set_armor_type(static_cast<ra2yrproto::ra2yr::Armor>(P->Armor));
}

void TypeClassParser::BuildingType() {
  TechnoType();
  auto* P = reinterpret_cast<BuildingTypeClass*>(c.src);
  T->set_array_index(P->ArrayIndex);
  T->set_power_drain(P->PowerDrain);
  T->set_power_bonus(P->PowerBonus);
  T->set_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_BUILDINGTYPE);
  T->set_is_base_defense(P->IsBaseDefense);
  T->set_wall(P->Wall);
  T->set_build_category(
      static_cast<ra2yrproto::ra2yr::BuildCategory>(P->BuildCat));
}

void TypeClassParser::parse() {
  T->set_pointer_self(reinterpret_cast<u32>(c.src));
  auto t = ra2::abi::AbstractClass_WhatAmI::call(
      c.abi, reinterpret_cast<AbstractClass*>(c.src));

  if (t == BuildingTypeClass::AbsID) {
    BuildingType();
  } else if (t == AircraftTypeClass::AbsID) {
    AircraftType();
  } else if (t == InfantryTypeClass::AbsID) {
    InfantryType();
  } else if (t == UnitTypeClass::AbsID) {
    UnitType();
  } else {
    eprintf("unknown TypeClass: {}", static_cast<int>(t));
  }
}

EventParser::EventParser(EventClass* src, ra2yrproto::ra2yr::Event* T, u32 time)
    : src(src), T(T), time(time) {}

void EventParser::MegaMission() {
  auto& x = src->Data.MegaMission;
  auto* d = T->mutable_mega_mission();
  auto* w = d->mutable_whom();
  w->set_m_id(x.Whom.m_ID);
  w->set_m_rtti(x.Whom.m_RTTI);
  d->set_mission(x.Mission);

  auto* tt = d->mutable_target();
  tt->set_m_id(x.Target.m_ID);
  tt->set_m_rtti(x.Target.m_RTTI);
  auto* td = d->mutable_destination();
  td->set_m_id(x.Destination.m_ID);
  td->set_m_rtti(x.Destination.m_RTTI);
  auto* tf = d->mutable_follow();
  tf->set_m_id(x.Follow.m_ID);
  tf->set_m_rtti(x.Follow.m_RTTI);
  d->set_is_planning_event(x.IsPlanningEvent);
}

void EventParser::MegaMission_F() {
  auto& x = src->Data.MegaMission_F;
  auto* d = T->mutable_mega_mission_f();
  auto* w = d->mutable_whom();
  w->set_m_id(x.Whom.m_ID);
  w->set_m_rtti(x.Whom.m_RTTI);
  d->set_mission(x.Mission);

  auto* tt = d->mutable_target();
  tt->set_m_id(x.Target.m_ID);
  tt->set_m_rtti(x.Target.m_RTTI);
  auto* td = d->mutable_destination();
  td->set_m_id(x.Destination.m_ID);
  td->set_m_rtti(x.Destination.m_RTTI);

  d->set_speed(x.Speed);
  d->set_max_speed(x.MaxSpeed);
}

void EventParser::Production() {
  auto& x = src->Data.Production;
  auto* d = T->mutable_production();
  d->set_rtti_id(x.RTTI_ID);
  d->set_heap_id(x.Heap_ID);
  d->set_is_naval(x.IsNaval);
}

void EventParser::Place() {
  auto& x = src->Data.Place;
  auto* d = T->mutable_place();
  d->set_rtti_type(static_cast<ra2yrproto::ra2yr::AbstractType>(x.RTTIType));
  d->set_heap_id(x.HeapID);
  d->set_is_naval(x.IsNaval);
  auto& ll = x.Location;
  auto* loc = d->mutable_location();
  loc->set_x(ll.X);
  loc->set_y(ll.Y);
}

void EventParser::parse() {
  T->set_event_type(static_cast<ra2yrproto::ra2yr::NetworkEvent>(src->Type));
  T->set_is_executed(src->IsExecuted);
  T->set_house_index(src->HouseIndex);
  T->set_frame(src->Frame);
  T->set_timing(time);
  switch (src->Type) {
    case EventType::PRODUCE:
      Production();
      break;
    case EventType::PLACE:
      Place();
      break;
    case EventType::MEGAMISSION:
      MegaMission();
      break;
    case EventType::MEGAMISSION_F:
      MegaMission_F();
      break;
    default:
      break;
  }
}

template <typename FnT>
static void apply_cells(MapClass* src, FnT fn) {
  auto* M = src;
  auto L = M->MapCoordBounds;

  for (int j = 0; j <= L.Bottom; j++) {
    for (int i = 0; i <= L.Right; i++) {
      CellStruct coords{static_cast<i16>(i), static_cast<i16>(j)};
      auto* src_cell = M->TryGetCellAt(coords);

      if (src_cell != nullptr) {
        fn(i, j, src_cell);
      }
    }
  }
}

template <unsigned N>
static bool bytes_equal(const void* p1, const void* p2) {
#if 0
  for (auto i = 0U; i < N; i++) {
    if (reinterpret_cast<const char*>(p1)[i] !=
        reinterpret_cast<const char*>(p2)[i]) {
      return false;
    }
  }
  return true;
#else
  return std::memcmp(p1, p2, N) == 0;
#endif
}

static void parse_Cell(Cell* C, const int ix, const CellClass& cc) {
  C->radiation_level = cc.RadLevel;
  C->land_type = static_cast<i32>(cc.LandType);
  C->height = cc.Height;
  C->level = cc.Level;
  C->overlay_data = cc.OverlayData;
  C->tiberium_value = 0;
  int ff = static_cast<int>(cc.AltFlags);
  C->shrouded = ((ff & 0x8) == 0);
  C->passability = cc.Passability;
  C->index = ix;
  if (C->land_type ==
      static_cast<int>(ra2yrproto::ra2yr::LandType::LAND_TYPE_Tiberium)) {
    C->tiberium_value = ra2::abi::get_tiberium_value(cc);
  }
}

std::vector<CellClass*> ra2::get_valid_cells(MapClass* M) {
  std::vector<CellClass*> res;
  auto L = M->MapCoordBounds;
  // Initialize valid cells
  for (int j = 0; j <= L.Bottom; j++) {
    for (int i = 0; i <= L.Right; i++) {
      CellStruct coords{static_cast<i16>(i), static_cast<i16>(j)};
      auto* cc = M->TryGetCellAt(coords);
      if (cc != nullptr) {
        res.push_back(cc);
      }
    }
  }
  return res;
}

static void parse_cells(Cell* dest, CellClass** src, const std::size_t c,
                        const LTRBStruct& L) {
  for (std::size_t k = 0; k < c; k++) {
    auto* cc = src[k];
    const int ix = (L.Right + 1) * (cc->MapCoords.Y) + (cc->MapCoords.X);

    auto& C = dest[k];
    parse_Cell(&C, ix, *cc);
  }
}

static void update_modified_cells(
    const Cell* current, Cell* previous, const std::size_t c,
    RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference) {
  for (std::size_t k = 0; k < c; k++) {
    auto& C = current[k];
    if (!bytes_equal<sizeof(Cell)>(&C, &previous[k])) {
      C.copy_to(difference->Add(), &C);
      previous[k] = C;
    }
  }
}

template <int N>
static void apply_cell_stride(
    Cell* previous, Cell* cell_buf, CellClass** cells,
    RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference, MapClass* M) {
  const auto& L = M->MapCoordBounds;
  parse_cells(cell_buf, cells, N, L);
  if (!bytes_equal<sizeof(Cell) * N>(cell_buf, previous)) {
    update_modified_cells(cell_buf, previous, N, difference);
  }
}

// TODO(shmocz): objects
void ra2::parse_map(std::vector<Cell>* previous, MapClass* D,
                    RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference) {
  static constexpr int chunk = 128;
  static std::vector<CellClass*> valid_cell_objects;
  static std::array<Cell, chunk> cell_buf;

  // Initialize valid cells
  if (valid_cell_objects.empty()) {
    valid_cell_objects = get_valid_cells(D);
  }

  auto* cellbuf = cell_buf.data();
  auto* cells = valid_cell_objects.data();
  Cell* prev_cells = previous->data();

  const auto sz = static_cast<int>(valid_cell_objects.size());
  const auto cnt = sz / chunk;
  for (int i = 0; i < cnt * chunk; i += chunk) {
    apply_cell_stride<chunk>(&prev_cells[i], cellbuf, &cells[i], difference, D);
  }

  for (int i = cnt * chunk; i < sz; i += chunk) {
    const auto& L = D->MapCoordBounds;
    auto c = std::min(chunk, (sz - cnt * chunk));
    parse_cells(cellbuf, &cells[i], c, L);
    if (!(std::memcmp(cellbuf, &prev_cells[i], c * sizeof(Cell)) == 0)) {
      update_modified_cells(cellbuf, &prev_cells[i], c, difference);
    }
  }
}

// TODO(shmocz): save only the utilized cells
void ra2::parse_MapData(ra2yrproto::ra2yr::MapData* dst, MapClass* src,
                        ra2::abi::ABIGameMD* abi) {
  auto* M = src;
  auto L = M->MapCoordBounds;
  auto sz = (L.Right + 1) * (L.Bottom + 1);
  auto* map_data = dst;
  map_data->set_width(L.Right + 1);
  map_data->set_height(L.Bottom + 1);
  auto* m = map_data->mutable_cells();
  if (m->size() != sz) {
    m->Clear();
    for (int i = 0; i < sz; i++) {
      m->Add();
    }
  }

  for (int j = 0; j <= L.Bottom; j++) {
    for (int i = 0; i <= L.Right; i++) {
      CellStruct coords{static_cast<i16>(i), static_cast<i16>(j)};
      auto* src_cell = M->TryGetCellAt(coords);

      if (src_cell != nullptr) {
        auto& c = m->at((L.Right + 1) * j + i);
        c.set_land_type(
            static_cast<ra2yrproto::ra2yr::LandType>(src_cell->LandType));
        c.set_height(src_cell->Height);
        c.set_level(src_cell->Level);
        c.set_radiation_level(src_cell->RadLevel);
        c.set_overlay_data(src_cell->OverlayData);
        if (src_cell->FirstObject != nullptr) {
          c.mutable_objects()->Clear();
          auto* o = c.add_objects();
          o->set_pointer_self(reinterpret_cast<u32>(src_cell->FirstObject));
        }
        if (c.land_type() == ra2yrproto::ra2yr::LandType::LAND_TYPE_Tiberium) {
          c.set_tiberium_value(
              ra2::abi::CellClass_GetContainedTiberiumValue::call(
                  abi, reinterpret_cast<std::uintptr_t>(src_cell)));
        }
        c.set_shrouded(ra2::abi::CellClass_IsShrouded::call(abi, src_cell));
        c.set_passability(src_cell->Passability);
      }
    }
  }
}

template <typename T>
void parse_EventList(RepeatedPtrField<ra2yrproto::ra2yr::Event>* dst, T* list) {
  if (dst->size() != list->Count) {
    dst->Clear();

    for (auto i = 0; i < list->Count; i++) {
      (void)dst->Add();
    }
  }
  for (auto i = 0; i < list->Count; i++) {
    auto ix =
        (list->Head + i) & ((sizeof(list->List) / sizeof(*list->List)) - 1);
    auto& it = dst->at(i);
    ra2::EventParser P(&list->List[ix], &it, list->Timings[ix]);
    P.parse();
  }
}

void ra2::parse_EventLists(ra2yrproto::ra2yr::GameState* G,
                           ra2yrproto::ra2yr::EventListsSnapshot* ES,
                           std::size_t max_size) {
  parse_EventList(G->mutable_out_list(), &EventClass::OutList.get());
  parse_EventList(G->mutable_do_list(), &EventClass::DoList.get());
  parse_EventList(G->mutable_megamission_list(),
                  &EventClass::MegaMissionList.get());
  auto* L = ES->mutable_lists();
  auto* F = ES->mutable_frame();

  // append
  ra2yrproto::ra2yr::EventLists EL;
  EL.mutable_out_list()->CopyFrom(G->out_list());
  EL.mutable_do_list()->CopyFrom(G->do_list());
  EL.mutable_megamission_list()->CopyFrom(G->megamission_list());
  // remove first if size exceeded
  if (L->size() >= static_cast<int>(max_size)) {
    L->erase(L->begin());
    F->erase(F->begin());
  }
  L->Add()->CopyFrom(EL);
  F->Add(G->current_frame());
}

void ra2::parse_prerequisiteGroups(ra2yrproto::ra2yr::PrerequisiteGroups* T) {
  auto& R = RulesClass::Instance;
  auto f = [](auto& L, auto* d) {
    auto IT = abi::DVCIterator(&L);
    for (auto id : IT) {
      d->Add(id);
    }
  };

  f(R->PrerequisiteProc, T->mutable_proc());
  f(R->PrerequisiteTech, T->mutable_tech());
  f(R->PrerequisiteRadar, T->mutable_radar());
  f(R->PrerequisiteBarracks, T->mutable_barracks());
  f(R->PrerequisiteFactory, T->mutable_factory());
  f(R->PrerequisitePower, T->mutable_power());
}

void ra2::parse_HouseClass(ra2yrproto::ra2yr::House* dst,
                           const HouseClass* src) {
  dst->set_array_index(src->ArrayIndex);
  dst->set_current_player(src->IsInPlayerControl);
  dst->set_defeated(src->Defeated);
  dst->set_is_game_over(src->IsGameOver);
  dst->set_is_loser(src->IsLoser);
  dst->set_is_winner(src->IsWinner);
  dst->set_money(src->Balance);
  dst->set_power_drain(src->PowerDrain);
  dst->set_power_output(src->PowerOutput);
  dst->set_start_credits(src->StartingCredits);
  dst->set_self(reinterpret_cast<std::uintptr_t>(src));
  dst->set_name(src->PlainName);
  dst->set_type_array_index(src->Type->ArrayIndex);
  dst->set_allied_infiltrated(src->Side0TechInfiltrated);
  dst->set_soviet_infiltrated(src->Side1TechInfiltrated);
  dst->set_third_infiltrated(src->Side2TechInfiltrated);
}

void ra2::parse_Factories(RepeatedPtrField<ra2yrproto::ra2yr::Factory>* dst) {
  auto* D = FactoryClass::Array.get();
  if (dst->size() != D->Count) {
    ra2yrcpp::protocol::fill_repeated_empty(dst, D->Count);
  }

  for (int i = 0; i < D->Count; i++) {
    auto* I = D->Items[i];
    auto& O = dst->at(i);
    O.set_object(reinterpret_cast<u32>(I->Object));
    O.set_owner(reinterpret_cast<u32>(I->Owner));
    O.set_progress_timer(I->Production.Value);
    O.set_on_hold(I->OnHold);
    auto A = ra2::abi::DVCIterator(&I->QueuedObjects);
    O.clear_queued_objects();
    for (auto* p : A) {
      O.add_queued_objects(reinterpret_cast<u32>(p));
    }
  }
}

RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>*
ra2::parse_AbstractTypeClasses(
    RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* T,
    ra2::abi::ABIGameMD* abi) {
  auto [D, T_] = init_arrays<AbstractTypeClass>(T);

  for (int i = 0; i < D->Count; i++) {
    // TODO: UB?
    ra2::TypeClassParser P({abi, D->Items[i]}, &T->at(i));
    P.parse();
  }
  return T;
}

void ra2::parse_Objects(ra2yrproto::ra2yr::GameState* G,
                        ra2::abi::ABIGameMD* abi) {
  auto [D, H] = init_arrays<TechnoClass>(G->mutable_objects());

  for (int i = 0; i < D->Count; i++) {
    ra2::ClassParser P({abi, D->Items[i]}, &H->at(i));
    P.parse();
  }
}

void ra2::parse_HouseClasses(ra2yrproto::ra2yr::GameState* G) {
  auto [D, H] = init_arrays<HouseClass>(G->mutable_houses());

  for (int i = 0; i < D->Count; i++) {
    ra2::parse_HouseClass(&H->at(i), D->Items[i]);
  }
}
