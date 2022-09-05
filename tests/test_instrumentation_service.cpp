#include "protocol/protocol.hpp"

#include "client_utils.hpp"
#include "common.hpp"
#include "gtest/gtest.h"
#include "util_string.hpp"
#include "utility/time.hpp"

#include <chrono>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace yrclient;
using ra2yrcpp::tests::InstrumentationServiceTest;

class IServiceTest : public InstrumentationServiceTest {
 protected:
  template <typename T>
  auto run(const T& cmd) {
    return client_utils::run(cmd, client.get());
  }
  void init() override {}
};

TEST_F(IServiceTest, HookingGetSetWorks) {
  // store initial flag value
  std::string key = "test_key";
  std::string flag1 = "0xdeadbeef";
  std::string flag2 = "0xbeefdead";
  auto value_eq = [&](std::string v) {
    yrclient::commands::GetValue g;
    g.mutable_args()->set_key(key);
    auto r = run(g);
    ASSERT_EQ(r, v);
  };

  yrclient::commands::StoreValue s;
  yrclient::commands::HookableCommand h;
  s.mutable_args()->set_key(key);
  s.mutable_args()->set_value(flag1);
  auto rrr = run(s);
  auto res0 = run(h);
  ASSERT_NE(res0.address_test_function(), 0);
  value_eq(flag1);
  yrclient::commands::InstallHook ih;
  auto* ih_a = ih.mutable_args();
  ih_a->set_address(res0.address_test_function());
  ih_a->set_name("test_hook");
  ih_a->set_code_length(res0.code_size());
  auto res_ih_a = run(ih);
  value_eq(flag1);
  // install callback, which modifies the value (TODO: jit the callback)
  yrclient::commands::AddCallback ac;
  ac.mutable_args()->set_hook_address(res0.address_test_function());
  ac.mutable_args()->set_callback_address(res0.address_test_callback());
  auto res_ac = run(ac);
  auto res1 = run(h);
  value_eq(flag2);
}
