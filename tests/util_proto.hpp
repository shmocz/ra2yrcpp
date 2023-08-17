#pragma once
#include "protocol/protocol.hpp"
#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/core.pb.h"

#include <string>
#include <vector>

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

struct HookableCommand {
  u64 address_test_function;
  u32 code_size;
  u64 address_test_callback;

  static ra2yrproto::commands::HookableCommand create(HookableCommand c) {
    ra2yrproto::commands::HookableCommand s;
    s.set_address_test_callback(c.address_test_callback);
    s.set_address_test_function(c.address_test_function);
    s.set_code_size(c.code_size);
    return s;
  }
};

struct AddCallback {
  u64 hook_address;
  u64 callback_address;

  static ra2yrproto::commands::AddCallback create(AddCallback c) {
    ra2yrproto::commands::AddCallback s;
    s.set_hook_address(c.hook_address);
    s.set_callback_address(c.callback_address);
    return s;
  }
};

struct CreateHooks {
  bool no_suspend_threads;
  std::vector<ra2yrproto::HookEntry> hooks;

  static ra2yrproto::commands::CreateHooks create(CreateHooks c) {
    ra2yrproto::commands::CreateHooks s;
    s.set_no_suspend_threads(c.no_suspend_threads);
    for (auto& ch : c.hooks) {
      auto* h = s.add_hooks();
      h->set_address(ch.address());
      h->set_name(ch.name());
      h->set_code_length(ch.code_length());
    }
    return s;
  }
};

};  // namespace ra2yrcpp::test_util
