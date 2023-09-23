#pragma once
#include "ra2yrproto/ra2yr.pb.h"

#include "ra2/abi.hpp"
#include "ra2/event_list.hpp"
#include "types.h"

#include <cstdint>

#include <functional>
#include <map>
#include <string>
#include <tuple>

namespace ra2 {

using abi_t = ra2::abi::ABIGameMD;

struct ObjectEntry {
  const ra2yrproto::ra2yr::Object* o;
  const ra2yrproto::ra2yr::ObjectTypeClass* tc;
};

/// Helper class to inspect protobuf state and raw game state.
/// Uses caching to perform fast lookups when mapping related objects
/// (e.g. Object's type to ObjectTypeClass instance).
class StateContext {
  using storage_t = ra2yrproto::ra2yr::StorageValue;
  using tc_cache_t =
      std::map<std::uintptr_t, const ra2yrproto::ra2yr::ObjectTypeClass*>;

 public:
  StateContext(abi_t* abi, storage_t* s);

  /// Add event to OutList.
  /// @param ev
  /// @param frame_delay how many frames to delay the execution relative to
  /// current frame
  /// @param spoof marks this event's House index to be spoofed. This requires
  /// additional patches to spawner.
  /// @param filter_duplicates throw exception if event of same type and
  /// house is found from any event list
  /// @return The resulting event added to the OutList
  /// @exception std::runtime_error if:
  /// - filter_duplicates is true and existing event was found
  /// - type is PRODUCE and no suitable type class was found
  /// - AddEvent returns false
  const EventEntry add_event(const ra2yrproto::ra2yr::Event& ev,
                             u32 frame_delay = 0U, bool spoof = false,
                             bool filter_duplicates = true);

  /// Retrieve ObjectTypeClass via pointer_self value.
  /// @param address value to be searched for
  /// @return pointer to the found ObjectTypeClass
  /// @exception std::runtime_error if not found
  const ra2yrproto::ra2yr::ObjectTypeClass* get_type_class(
      std::uintptr_t address);

  /// Find event with matching type and house index from all event lists.
  /// @param query event to be matched against
  /// @return the event entry and list name
  /// @exception
  const std::tuple<EventEntry, std::string> find_event(
      const ra2yrproto::ra2yr::Event& query);

  const ra2yrproto::ra2yr::Object* get_object(
      std::function<bool(const ra2yrproto::ra2yr::Object&)> pred);

  /// Find object from current state that matches a predicate.
  /// @param pred predicate that returns true on match, false on failure
  /// @exception std::runtime_error if no object was found
  ObjectEntry get_object_entry(
      std::function<bool(const ra2::ObjectEntry&)> pred);

  /// Find object from current state by address value.
  /// @param address
  /// @exception std::runtime_error if no object was found
  ObjectEntry get_object_entry(std::uintptr_t address);

  ObjectEntry get_object_entry(const ra2yrproto::ra2yr::Object& O);

  const ra2yrproto::ra2yr::House* get_house(
      std::function<bool(const ra2yrproto::ra2yr::House&)> pred);

  const ra2yrproto::ra2yr::House* get_house(std::uintptr_t address);

  const ra2yrproto::ra2yr::GameState* current_state();
  const ra2yrproto::ra2yr::House* current_player();

  const ra2yrproto::ra2yr::Factory* find_factory(
      std::function<bool(const ra2yrproto::ra2yr::Factory&)> pred);

  tc_cache_t& tc_cache();

  bool can_place(const ra2yrproto::ra2yr::House& H,
                 const ra2yrproto::ra2yr::ObjectTypeClass& T,
                 const ra2yrproto::ra2yr::Coordinates& C);

  abi_t* abi_;
  storage_t* s_;
  tc_cache_t tc_cache_;

 private:
  const ra2yrproto::ra2yr::Object* get_object(std::uintptr_t address);
};
}  // namespace ra2
