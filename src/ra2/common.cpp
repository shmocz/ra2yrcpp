#include "ra2/common.hpp"

#include "ra2yrproto/ra2yr.pb.h"

#include "ra2/yrpp_export.hpp"

using namespace ra2;

CellStruct ra2::coord_to_cell(const ra2yrproto::ra2yr::Coordinates& c) {
  CoordStruct coords{.X = c.x(), .Y = c.y(), .Z = c.z()};
  return CellClass::Coord2Cell(coords);
}

CellClass* ra2::get_map_cell(const ra2yrproto::ra2yr::Coordinates& c) {
  return MapClass::Instance.get()->TryGetCellAt(coord_to_cell(c));
}
