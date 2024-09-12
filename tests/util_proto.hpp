#pragma once
#include "protocol/protocol.hpp"
#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/core.pb.h"

#include <string>

namespace ra2yrcpp::test_util {

struct StoreValue {
  std::string key;
  std::string value;

  static ra2yrproto::commands::StoreValue create(StoreValue c) {
    ra2yrproto::commands::StoreValue s;
    s.set_key(c.key);
    s.set_value(c.value);
    return s;
  }
};

struct GetValue {
  std::string key;
  std::string value;

  static ra2yrproto::commands::GetValue create(GetValue c) {
    ra2yrproto::commands::GetValue s;
    s.set_key(c.key);
    s.set_value(c.value);
    return s;
  }
};

};  // namespace ra2yrcpp::test_util
