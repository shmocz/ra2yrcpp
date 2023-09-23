#include "ra2/state_parser.hpp"

#include "ra2yrproto/ra2yr.pb.h"

#include "config.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "ra2/abi.hpp"
#include "ra2/event_list.hpp"
#include "ra2/yrpp_export.hpp"
#include "utility/array_iterator.hpp"
#include "utility/serialize.hpp"

#include <fmt/core.h>

#undef GetMessage

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <stdexcept>

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
  if (src->first_object != 0U) {
    dst->mutable_objects()->Clear();
    dst->add_objects()->set_pointer_self(src->first_object);
  }
}

void ClassParser::Object() {
  auto* P = reinterpret_cast<ObjectClass*>(c.src);
  T->set_health(P->Health);
  T->set_selected(P->IsSelected);
  T->set_in_limbo(P->InLimbo);
  if (P->IsOnMap) {
    auto* q = T->mutable_coordinates();
    auto L = P->Location;
    q->set_x(L.X);
    q->set_y(L.Y);
    q->set_z(L.Z);
  }
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
  set_type_class(P->Type, ra2yrproto::ra2yr::ABSTRACT_TYPE_AIRCRAFT);
}

void ClassParser::Unit() {
  Foot();
  auto* P = reinterpret_cast<UnitClass*>(c.src);
  set_type_class(P->Type, ra2yrproto::ra2yr::ABSTRACT_TYPE_UNIT);
  T->set_deployed(P->Deployed);
  T->set_deploying(P->IsDeploying);
}

void ClassParser::Building() {
  Techno();
  auto* P = reinterpret_cast<BuildingClass*>(c.src);
  set_type_class(P->Type, ra2yrproto::ra2yr::ABSTRACT_TYPE_BUILDING);
}

void ClassParser::Infantry() {
  Foot();
  auto* P = reinterpret_cast<InfantryClass*>(c.src);
  set_type_class(P->Type, ra2yrproto::ra2yr::ABSTRACT_TYPE_INFANTRY);
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
    // eprintf("unknown ObjectClass: {}", static_cast<int>(t));
  }
}

void ClassParser::set_type_class(void* ttc, ra2yrproto::ra2yr::AbstractType t) {
  T->set_object_type(t);
  T->set_pointer_technotypeclass(reinterpret_cast<u32>(ttc));
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
  T->set_strength(P->Strength);
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

EventParser::EventParser(const EventClass* src, ra2yrproto::ra2yr::Event* T,
                         u32 time)
    : src(src), T(T), time(time) {}

static void parse_TargetClass(const TargetClass& s,
                              ra2yrproto::ra2yr::TargetClass* d) {
  d->set_m_id(s.m_ID);
  d->set_m_rtti(s.m_RTTI);
}

template <typename EventT, typename MessageT>
static auto parse_MegaMission_common(const EventT& e, MessageT* m) {
  parse_TargetClass(e.Whom, m->mutable_whom());
  m->set_mission(static_cast<ra2yrproto::ra2yr::Mission>(e.Mission));
  parse_TargetClass(e.Target, m->mutable_target());
  parse_TargetClass(e.Destination, m->mutable_destination());
  return std::make_tuple(e, m);
}

void EventParser::MegaMission() {
  if (src->Type == EventType::MEGAMISSION) {
    auto [x, m] = parse_MegaMission_common(src->Data.MegaMission,
                                           T->mutable_mega_mission());
    parse_TargetClass(x.Follow, m->mutable_follow());
    m->set_is_planning_event(x.IsPlanningEvent);
  } else if (src->Type == EventType::MEGAMISSION_F) {
    auto [x, m] = parse_MegaMission_common(src->Data.MegaMission_F,
                                           T->mutable_mega_mission_f());
    m->set_speed(x.Speed);
    m->set_max_speed(x.MaxSpeed);
  } else {
    throw std::runtime_error(fmt::format(
        "invalid event type: {}", static_cast<unsigned char>(src->Type)));
  }
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
      MegaMission();
      break;
    default:
      break;
  }
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
  C->first_object = reinterpret_cast<std::uintptr_t>(cc.FirstObject);
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
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference) {
  for (std::size_t k = 0; k < c; k++) {
    auto& C = current[k];
    if (!serialize::bytes_equal(&C, &previous[k])) {
      C.copy_to(difference->Add(), &C);
      previous[k] = C;
    }
  }
}

template <int N>
static void apply_cell_stride(
    Cell* previous, Cell* cell_buf, CellClass** cells,
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference, MapClass* M) {
  const auto& L = M->MapCoordBounds;
  parse_cells(cell_buf, cells, N, L);
  if (!serialize::bytes_equal(cell_buf, previous, sizeof(Cell) * N)) {
    update_modified_cells(cell_buf, previous, N, difference);
  }
}

// TODO(shmocz): objects
void ra2::parse_map(
    std::vector<Cell>* previous, MapClass* D,
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::Cell>* difference) {
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
static void parse_EventList(
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::Event>* dst, T* list) {
  if (dst->size() != list->Count) {
    dst->Clear();

    for (auto i = 0; i < list->Count; i++) {
      (void)dst->Add();
    }
  }
  EventListUtil::apply(list, [dst](const EventEntry& e) {
    auto& it = dst->at(e.index);
    ra2::EventParser P(e.e, &it, e.timing);
    P.parse();
    return false;
  });
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
  dst->set_is_human_player(src->IsHumanPlayer);
}

void ra2::parse_Factories(
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::Factory>* dst) {
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
    O.set_completed(O.progress_timer() == cfg::PRODUCTION_STEPS);
    auto A = ra2::abi::DVCIterator(&I->QueuedObjects);
    O.clear_queued_objects();
    for (auto* p : A) {
      O.add_queued_objects(reinterpret_cast<u32>(p));
    }
  }
}

gpb::RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>*
ra2::parse_AbstractTypeClasses(
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* T,
    ra2::abi::ABIGameMD* abi) {
  auto [D, T_] = init_arrays<AbstractTypeClass>(T);

  for (int i = 0; i < D->Count; i++) {
    // TODO: UB?
    ra2::TypeClassParser P({abi, D->Items[i]}, &T->at(i));
    P.parse();
  }
  return T;
}

// Insert msg to repeated ptr field, resizing the field if necessary
template <typename T>
static T* try_get_message(gpb::RepeatedPtrField<T>* f, int index,
                          int capacity_increment = 10) {
  if (f->size() < (index + 1)) {
    ra2yrcpp::protocol::fill_repeated(f, capacity_increment);
  }
  return &f->at(index);
}

// TODO: const corr
template <typename T, typename D>
int parse_object_array(ra2::abi::ABIGameMD* abi, T* src,
                       gpb::RepeatedPtrField<D>* dst, int j) {
  ra2yrproto::ra2yr::Object* O = nullptr;
  for (int i = 0; i < src->Count; i++) {
    if (O == nullptr) {
      O = try_get_message(dst, j);
    }
    try {
      ra2::ClassParser P({abi, src->Items[i]}, O);
      P.parse();
      j++;
      O = nullptr;
    } catch (...) {
    }
  }
  return j;
}

void ra2::parse_Objects(ra2yrproto::ra2yr::GameState* G,
                        ra2::abi::ABIGameMD* abi) {
  auto* H = G->mutable_objects();
  (void)ra2yrcpp::protocol::truncate(
      H, parse_object_array(abi, TechnoClass::Array.get(), H, 0));
}

void ra2::parse_HouseClasses(ra2yrproto::ra2yr::GameState* G) {
  auto [D, H] = init_arrays<HouseClass>(G->mutable_houses());

  for (int i = 0; i < D->Count; i++) {
    ra2::parse_HouseClass(&H->at(i), D->Items[i]);
  }
}

ra2yrproto::ra2yr::ObjectTypeClass* ra2::find_type_class(
    gpb::RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* types,
    ra2yrproto::ra2yr::AbstractType rtti_id, int array_index) {
  auto e = std::find_if(types->begin(), types->end(), [&](const auto& v) {
    return v.array_index() == array_index && v.type() == rtti_id;
  });
  return e != types->end() ? &(*e) : nullptr;
}

bool ra2::is_local(const gpb::RepeatedPtrField<ra2yrproto::ra2yr::House>& H) {
  return std::count_if(H.begin(), H.end(),
                       [](const auto& h) { return h.is_human_player(); }) == 1;
}
