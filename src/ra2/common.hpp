#pragma once

#include "ra2/yrpp_export.hpp"

namespace ra2yrproto {
namespace ra2yr {
class Coordinates;
}
}  // namespace ra2yrproto

namespace ra2 {
CellStruct coord_to_cell(const ra2yrproto::ra2yr::Coordinates& c);
CellClass* get_map_cell(const ra2yrproto::ra2yr::Coordinates& c);
};  // namespace ra2
