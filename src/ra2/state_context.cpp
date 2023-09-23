#include "ra2/state_context.hpp"

#include "ra2/common.hpp"

#include <stdexcept>

#undef ERROR
#include "ra2yrproto/ra2yr.pb.h"

#include "logging.hpp"
#include "ra2/state_parser.hpp"
#include "ra2/yrpp_export.hpp"
#include "types.h"

#include <fmt/core.h>
#include <google/protobuf/repeated_ptr_field.h>

#include <cstdint>

#include <algorithm>
#include <functional>
#include <string>
#include <tuple>

namespace gpb = google::protobuf;

using namespace ra2;

template <typename T>
static const T* find_entry(const gpb::RepeatedPtrField<T>& E,
                           std::function<bool(const T&)> pred) {
  auto i = std::find_if(E.begin(), E.end(), pred);
  return i == E.end() ? nullptr : &(*i);
}

StateContext::StateContext(abi_t* abi, storage_t* s) : abi_(abi), s_(s) {}

const EventEntry StateContext::add_event(const ra2yrproto::ra2yr::Event& ev,
                                         u32 frame_delay, bool spoof,
                                         bool filter_duplicates) {
  if (filter_duplicates) {
    const auto [me, name] = find_event(ev);
    dprintf("adding event {}", ev.ShortDebugString());
    if (me.e != nullptr) {
      throw std::runtime_error(fmt::format("event already exists in {}, src={}",
                                           name, ev.ShortDebugString()));
    }
  }

  int frame = Unsorted::CurrentFrame + static_cast<int>(frame_delay);
  auto house_index = static_cast<char>(
      spoof ? ev.house_index() : HouseClass::CurrentPlayer->ArrayIndex);
  // This is how the frame is computed for protocol zero.
  if (frame_delay == 0) {
    const int& fsr = Game::Network::FrameSendRate;
    frame =
        (((fsr + Unsorted::CurrentFrame - 1 + Game::Network::MaxAhead) / fsr) *
         fsr);
  }
  // Set the frame to negative value to indicate that house index and
  // frame number should be spoofed
  frame = frame * (spoof ? -1 : 1);

  EventClass E(static_cast<EventType>(ev.event_type()), false, house_index,
               static_cast<u32>(frame));

  const auto ts = abi_->timeGetTime();
  if (ev.has_production()) {
    auto& e = ev.production();
    if (ra2::find_type_class(
            s_->mutable_initial_game_state()->mutable_object_types(),
            static_cast<ra2yrproto::ra2yr::AbstractType>(e.rtti_id()),
            e.heap_id()) == nullptr) {
      throw std::runtime_error(
          fmt::format("invalid id: RTTI={},heap={}", e.rtti_id(), e.heap_id()));
    }
    E.Data.Production = {.RTTI_ID = e.rtti_id(),
                         .Heap_ID = e.heap_id(),
                         .IsNaval = e.is_naval()};

  } else if (ev.has_place()) {
    auto& e = ev.place();
    auto loc = e.location();
    auto S = CoordStruct{.X = loc.x(), .Y = loc.y(), .Z = loc.z()};
    E.Data.Place = {.RTTIType = static_cast<AbstractType>(e.rtti_type()),
                    .HeapID = e.heap_id(),
                    .IsNaval = e.is_naval(),
                    .Location = CellClass::Coord2Cell(S)};
  }
  if (!EventClass::AddEvent(E, static_cast<int>(ts))) {
    throw std::runtime_error("failed to add event");
  }

  const auto& EL = EventClass::OutList.get();
  return EventListUtil::convert_event(&EL, &EL.List[(EL.Tail - 1) & 127]);
}

const ra2yrproto::ra2yr::ObjectTypeClass* StateContext::get_type_class(
    std::uintptr_t address) {
  try {
    return tc_cache().at(address);
  } catch (const std::out_of_range& e) {
    throw std::runtime_error(fmt::format("invalid type class {}", address));
  }
}

const ra2yrproto::ra2yr::Object* StateContext::get_object(
    std::function<bool(const ra2yrproto::ra2yr::Object&)> pred) {
  return find_entry(s_->game_state().objects(), pred);
}

const ra2yrproto::ra2yr::Object* StateContext::get_object(
    std::uintptr_t address) {
  const auto* O =
      get_object([&](const auto& v) { return v.pointer_self() == address; });
  if (O == nullptr) {
    throw std::runtime_error(fmt::format("object {} not found", address));
  }
  return O;
}

ObjectEntry StateContext::get_object_entry(
    std::function<bool(const ra2::ObjectEntry&)> pred) {
  ObjectEntry res{};
  auto* O = get_object([&](const ra2yrproto::ra2yr::Object& v) {
    res = {&v, get_type_class(v.pointer_technotypeclass())};
    return pred(res);
  });
  if (O == nullptr) {
    throw std::runtime_error("object not found");
  }
  return res;
}

ObjectEntry StateContext::get_object_entry(std::uintptr_t address) {
  const auto* o = get_object(address);
  const auto* t = get_type_class(o->pointer_technotypeclass());
  return {o, t};
}

ObjectEntry StateContext::get_object_entry(const ra2yrproto::ra2yr::Object& O) {
  return get_object_entry(O.pointer_self());
}

const ra2yrproto::ra2yr::House* StateContext::get_house(
    std::function<bool(const ra2yrproto::ra2yr::House&)> pred) {
  return find_entry(current_state()->houses(), pred);
}

const ra2yrproto::ra2yr::House* StateContext::get_house(
    std::uintptr_t address) {
  return get_house([address](const auto& v) { return v.self() == address; });
}

const ra2yrproto::ra2yr::GameState* StateContext::current_state() {
  return &s_->game_state();
}

const ra2yrproto::ra2yr::House* StateContext::current_player() {
  return get_house([](const auto& v) { return v.current_player(); });
}

StateContext::tc_cache_t& StateContext::tc_cache() {
  if (tc_cache_.empty()) {
    for (const auto& O : s_->initial_game_state().object_types()) {
      tc_cache_.emplace(O.pointer_self(), &O);
    }
  }
  return tc_cache_;
}

const std::tuple<EventEntry, std::string> StateContext::find_event(
    const ra2yrproto::ra2yr::Event& query) {
  EventClass E(static_cast<EventType>(query.event_type()), false,
               static_cast<char>(query.house_index()), query.frame());

  EventEntry r{};
  r = EventListUtil::find(EventListType::OUT_LIST, E);
  if (r.e != nullptr) {
    return std::make_tuple(r, "OutList");
  }
  r = EventListUtil::find(EventListType::DO_LIST, E);
  if (r.e != nullptr) {
    return std::make_tuple(r, "DoList");
  }

  r = EventListUtil::find(EventListType::MEGAMISSION_LIST, E);
  if (r.e != nullptr) {
    return std::make_tuple(r, "MegaMissionList");
  }
  return std::make_tuple(r, "");
}

const ra2yrproto::ra2yr::Factory* StateContext::find_factory(
    std::function<bool(const ra2yrproto::ra2yr::Factory&)> pred) {
  return find_entry(s_->game_state().factories(), pred);
}

bool StateContext::can_place(const ra2yrproto::ra2yr::House& H,
                             const ra2yrproto::ra2yr::ObjectTypeClass& T,
                             const ra2yrproto::ra2yr::Coordinates& C) {
  const std::uintptr_t p_DisplayClass = 0x87F7E8u;
  auto cell_s = ra2::coord_to_cell(C);
  return (abi_->DisplayClass_Passes_Proximity_Check(
              p_DisplayClass,
              reinterpret_cast<BuildingTypeClass*>(T.pointer_self()),
              current_player()->array_index(), &cell_s) &&
          abi_->BuildingTypeClass_CanPlaceHere(T.pointer_self(), &cell_s, H.self()));
}
