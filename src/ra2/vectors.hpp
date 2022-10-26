#pragma once
#include "ra2/general.h"
#include "utility/serialize.hpp"

#include <iostream>
#include <vector>

namespace ra2 {
namespace vectors {

namespace {
using namespace ra2::general;
using serialize::read_obj;
}  // namespace

template <typename T>
struct Vec3D {
  T x, y, z;
  template <typename U>
  friend std::ostream& operator<<(std::ostream& os, const Vec3D<U>& o);
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const Vec3D<T>& o) {
  os << "(" << o.x << "," << o.y << "," << o.z << ")";
  return os;
}

template <typename T>
struct Vec2D {
  T x, y;
};

template <typename T>
struct Vec4D {
  T x, y, z, w;
};

struct Matrix3D {
  union {
    Vec4D<float> Row[3];
    float row[3][4];
    float Data[12];
  };

  std::vector<float> as_vector();
};

template <typename T>
struct VectorClass {
  void* vtable;
  T* Items{nullptr};
  i32 Capacity{0};
  ybool IsInitialized{true};
  ybool IsAllocated{false};
};

using CellStruct = Vec2D<i16>;
using CoordStruct = Vec3D<i32>;
using Edge = i32;

template <typename T>
struct DynamicVectorClass : public VectorClass<T> {
  i32 Count{0};
  i32 CapacityIncrement{10};
  template <typename U>
  friend std::ostream& operator<<(std::ostream& os,
                                  const DynamicVectorClass<U>& V);
};

}  // namespace vectors
}  // namespace ra2
