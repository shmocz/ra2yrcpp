#pragma once

#include "ra2/abstract_types.hpp"
#include "ra2/general.h"
#include "ra2/type_classes.hpp"
#include "ra2/vectors.hpp"
#include "types.h"

namespace ra2 {
namespace game_screen {

namespace {
using ra2::general::LTRBStruct;
using ra2::general::RectangleStruct;
using ra2::vectors::CellStruct;
}  // namespace

struct CellClass : public ra2::abstract_types::NewAbstractClass {
  ra2::vectors::CellStruct MapCoords;
  // TODO: remaining members
};

struct GScreenClass {
  void* vtable;
  i32 ScreenShakeX;
  i32 ScreenShakeY;
  i32 Bitfield;  // default is 2
};

// TODO(shmocz): implement
struct SubzoneTrackingStruct {};

// TODO(shmocz): implement
struct ZoneConnectionClass {};

// TODO(shmocz): implement
struct CellLevelPassabilityStruct {};

// TODO(shmocz): implement
struct LevelAndPassabilityStruct2 {};

struct MapClass : public GScreenClass {
  static constexpr std::uintptr_t instance = 0x87F7E8U;
  static const int MaxCells = 0x40000;
  // TODO: padding
  u32 unknown_10;
  void* unknown_pointer_14;
  void* MovementZones[13];
  u32 somecount_4C;
  ra2::vectors::DynamicVectorClass<ZoneConnectionClass> ZoneConnections;
  CellLevelPassabilityStruct* LevelAndPassability;
  int ValidMapCellCount;
  LevelAndPassabilityStruct2* LevelAndPassabilityStruct2pointer_70;
  u32 unknown_74;
  u32 unknown_78;
  u32 unknown_7C;
  u32 unknown_80[3];  // somehow connected to the 3 vectors below

  ra2::vectors::DynamicVectorClass<SubzoneTrackingStruct> SubzoneTracking1;
  ra2::vectors::DynamicVectorClass<SubzoneTrackingStruct> SubzoneTracking2;
  ra2::vectors::DynamicVectorClass<SubzoneTrackingStruct> SubzoneTracking3;
  ra2::vectors::DynamicVectorClass<CellStruct> CellStructs1;
  RectangleStruct MapRect;
  RectangleStruct VisibleRect;
  int CellIterator_NextX;
  int CellIterator_NextY;
  int CellIterator_CurrentY;
  CellClass* CellIterator_NextCell;
  int ZoneIterator_X;
  int ZoneIterator_Y;
  LTRBStruct MapCoordBounds;  // the minimum and maximum cell struct values
  int TotalValue;
  ra2::vectors::VectorClass<CellClass*> Cells;

  static int GetCellIndex(const CellStruct& MapCoords) {
    return (MapCoords.y << 9) + MapCoords.x;
  }

  static CellClass* TryGetCellAt(const CellStruct& MapCoords) {
    int idx = GetCellIndex(MapCoords);
    return (idx >= 0 && idx < MaxCells)
               ? reinterpret_cast<MapClass*>(MapClass::instance)
                     ->Cells.Items[idx]
               : nullptr;
  }
};

}  // namespace game_screen
}  // namespace ra2
