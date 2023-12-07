#include "ra2/event_list.hpp"

#include "ra2/yrpp_export.hpp"
#include "utility/serialize.hpp"

#include <stdexcept>

using namespace ra2;

void EventListUtil::elist_apply(EventListCtx* C,
                                std::function<bool(const EventEntry& e)> fn) {
  for (auto i = 0; i < C->count; i++) {
    auto e = get_event(C, i);
    if (fn(e)) {
      break;
    }
  }
}

EventListCtx EventListUtil::from_eventlist(EventListType t) {
  switch (t) {
    case OUT_LIST:
      return EventListCtx::out_list();
    case DO_LIST:
      return EventListCtx::do_list();
    case MEGAMISSION_LIST:
      return EventListCtx::megamission_list();
    default:
      throw std::runtime_error("Invalid EventList type");
  }
}

EventEntry EventListUtil::find(EventListType t, const EventClass& E) {
  EventEntry res{};
  auto C = EventListUtil::from_eventlist(t);
  auto fn = [&](const EventEntry& e) {
    if (e.e->HouseIndex == E.HouseIndex && e.e->Type == E.Type) {
      if (E.Type == EventType::SELLCELL) {
        if (serialize::bytes_equal(&E.Data.SellCell, &e.e->Data.SellCell)) {
          res = e;
          return true;
        }
      } else if (E.Type == EventType::PLACE) {
        if (E.Data.Place.RTTIType == e.e->Data.Place.RTTIType &&
            E.Data.Place.HeapID == e.e->Data.Place.HeapID) {
          res = e;
          return true;
        }
      } else {
        res = e;
        return true;
      }
    }
    return false;
  };
  elist_apply(&C, fn);
  return res;
}
