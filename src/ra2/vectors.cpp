#include "ra2/vectors.hpp"

using namespace ra2::vectors;

CellStruct ra2::vectors::Coord2Cell(const CoordStruct crd) {
  return {.x = static_cast<i16>(crd.x / 256),
          .y = static_cast<i16>(crd.y / 256)};
}
