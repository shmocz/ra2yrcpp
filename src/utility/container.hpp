#pragma once
#include "debug_helpers.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace utility {

///
/// Apply function op to each element of container U and store results into a
/// vector.
///
template <typename U, typename O>
inline auto vmap(U input, O op) {
  using ResT =
      std::vector<typename std::result_of<O(typename U::value_type)>::type>;
  if (input.size() == 0) {
    return ResT{};
  }
  ResT res(input.size());
  std::transform(input.begin(), input.end(), res.begin(), op);
  return res;
}
};  // namespace utility
