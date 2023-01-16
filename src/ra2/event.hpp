#pragma once
#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/type_classes.hpp"
#include "ra2/vectors.hpp"
#include "types.h"

#include <functional>

namespace ra2 {
namespace event {

#pragma pack(push, 1)

struct TargetClass {
  int m_ID;
  unsigned char m_RTTI;
};

// Pretty much copypaste from YRpp
enum class EventType : unsigned char {
  EMPTY = 0,
  POWERON = 1,
  POWEROFF = 2,
  ALLY = 3,
  MEGAMISSION = 4,
  MEGAMISSION_F = 5,
  IDLE = 6,
  SCATTER = 7,
  DESTRUCT = 8,
  DEPLOY = 9,
  DETONATE = 10,
  PLACE = 11,
  OPTIONS = 12,
  GAMESPEED = 13,
  PRODUCE = 14,
  SUSPEND = 15,
  ABANDON = 16,
  PRIMARY = 17,
  SPECIAL_PLACE = 18,
  EXIT = 19,
  ANIMATION = 20,
  REPAIR = 21,
  SELL = 22,
  SELLCELL = 23,
  SPECIAL = 24,
  FRAMESYNC = 25,
  MESSAGE = 26,
  RESPONSE_TIME = 27,
  FRAMEINFO = 28,
  SAVEGAME = 29,
  ARCHIVE = 30,
  ADDPLAYER = 31,
  TIMING = 32,
  PROCESS_TIME = 33,
  PAGEUSER = 34,
  REMOVEPLAYER = 35,
  LATENCYFUDGE = 36,
  MEGAFRAMEINFO = 37,
  PACKETTIMING = 38,
  ABOUTTOEXIT = 39,
  FALLBACKHOST = 40,
  ADDRESSCHANGE = 41,
  PLANCONNECT = 42,
  PLANCOMMIT = 43,
  PLANNODEDELETE = 44,
  ALLCHEER = 45,
  ABANDON_ALL = 46,
  LAST_EVENT = 47,
};

union EventData {
  struct {
    char Data[104];
  } SpaceGap;  // Just a space gap to align the struct

  struct {
    int ID;         // Anim ID
    int AnimOwner;  // House ID
    ra2::vectors::CellStruct Location;
  } Animation;

  struct {
    unsigned int CRC;
    u16 CommandCount;
    unsigned char Delay;
  } FrameInfo;

  struct {
    TargetClass Whom;
  } Target;

  struct {
    TargetClass Whom;
    unsigned char Mission;  // Mission but should be byte
    char _gap_;
    TargetClass Target;
    TargetClass Destination;
    TargetClass Follow;
    bool IsPlanningEvent;
  } MegaMission;

  struct {
    TargetClass Whom;
    unsigned char Mission;
    TargetClass Target;
    TargetClass Destination;
    int Speed;
    int MaxSpeed;
  } MegaMission_F;  // Seems unused in YR?

  struct {
    int RTTI_ID;
    int Heap_ID;
    int IsNaval;
  } Production;

  struct {
    int Unknown_0;
    i64 Data;
    int Unknown_C;
  } Unknown_LongLong;

  struct {
    int Unknown_0;
    int Unknown_4;
    int Data;
    int Unknown_C;
  } Unknown_Tuple;

  struct {
    ra2::general::AbstractType RTTIType;
    int HeapID;
    int IsNaval;
    ra2::vectors::CellStruct Location;
  } Place;

  struct {
    int ID;
    ra2::vectors::CellStruct Location;
  } SpecialPlace;

  struct {
    ra2::general::AbstractType RTTIType;
    int ID;
  } Specific;
};

struct EventClass {
  // static constexpr reference<const char*, 0x82091C, 47> const EventNames{};

  EventType Type;
  bool IsExecuted;
  char HouseIndex;  // '-1' stands for not a valid house
  u32 Frame;        // 'Frame' is the frame that the command should execute on.

  EventData Data;
  static char* get_name(EventType t);
};

static_assert(sizeof(EventClass) == 111);

#pragma pack(pop)

template <int Length, unsigned Address = 0U>
struct EventList {
  static const auto length = Length;
  static const auto address = Address;

 public:
  int Count;
  int Head;
  int Tail;
  EventClass List[Length];
  int Timings[Length];
};

using OutList = EventList<0x80, 0xA802C8>;
using DoList = EventList<0x4000, 0x8B41F8>;
using MegaMission = EventList<0x100, 0xA83ED0>;

template <typename EventLT>
bool AddEvent(const EventClass& event, const u32 timestamp) {
  auto* list = reinterpret_cast<EventList<EventLT::length>*>(EventLT::address);
  if (list->Count >= EventLT::length) {
    return false;
  }

  list->List[list->Tail] = event;

  list->Timings[list->Tail] = timestamp;

  ++list->Count;
  list->Tail = (list->Tail + 1) & 127;
  return true;
}

}  // namespace event
}  // namespace ra2
