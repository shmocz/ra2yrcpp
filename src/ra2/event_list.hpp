#pragma once
#include "ra2/yrpp_export.hpp"

#include <functional>

namespace ra2 {
struct EventEntry {
  const EventClass* e;
  int timing;
  int index;
};

enum EventListType : int { OUT_LIST = 1, DO_LIST = 2, MEGAMISSION_LIST = 3 };

struct EventListCtx {
  int count;
  int head;
  int tail;
  EventClass* list;
  int* timings;
  int length;

  template <typename EListT>
  static EventListCtx from_eventlist(EListT* E) {
    return {E->Count, E->Head,    E->Tail,
            E->List,  E->Timings, (sizeof(E->List) / sizeof(*E->List))};
  }

  static EventListCtx out_list() {
    return from_eventlist(&EventClass::OutList.get());
  }

  static EventListCtx do_list() {
    return from_eventlist(&EventClass::DoList.get());
  }

  static EventListCtx megamission_list() {
    return from_eventlist(&EventClass::MegaMissionList.get());
  }
};

struct EventListUtil {
  static EventEntry get_event(EventListCtx* C, int i) {
    auto ix = static_cast<int>((C->head + i) & (C->length - 1));
    return EventEntry{&C->list[ix], C->timings[ix], i};
  }

  template <typename EListT>
  static EventEntry convert_event(EListT* list, const EventClass* E) {
    auto ix = (E - list->List) / sizeof(*E);
    // FIXME  wrong ix
    return EventEntry{&list->List[ix], list->Timings[ix], static_cast<int>(ix)};
  }

  static void elist_apply(EventListCtx* C,
                          std::function<bool(const EventEntry& e)> fn);

  template <typename EListT>
  static void apply(EListT* list, std::function<bool(const EventEntry& e)> fn) {
    auto C = EventListCtx::from_eventlist(list);
    return elist_apply(&C, fn);
  }

  static EventListCtx from_eventlist(EventListType t);

  static EventEntry find(EventListType t, const EventClass& E);
};

};  // namespace ra2
