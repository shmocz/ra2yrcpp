#include "gtest/gtest.h"
#include "config.hpp"
#include "debug_helpers.h"
#include "protocol/protocol.hpp"
#include "connection.hpp"
#include "util_string.hpp"
#include "utility/time.hpp"
#include "instrumentation_service.hpp"
#include "instrumentation_client.hpp"
#include "client_utils.hpp"
#include <unistd.h>
#include <chrono>
#include <vector>
#include <thread>
#include <cassert>
#include <memory>

using namespace yrclient;
using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;

class IServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    I = std::make_unique<yrclient::InstrumentationService>(cfg::MAX_CLIENTS,
                                                           cfg::SERVER_PORT);
    auto& S = I->server();
    C_ = std::make_unique<connection::Connection>(S.address(), S.port());
    client = std::make_unique<InstrumentationClient>(C_.get(), 5000ms, 500ms);
  }

  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<connection::Connection> C_;
  std::unique_ptr<InstrumentationClient> client;
  // void TearDown() override {}
  template <typename T>
  std::string run_one(const T& msg) {
    auto r = client->run_one(msg);
    T aa;
    r.body().UnpackTo(&aa);
    return aa.result();
  }
  auto run_hookable() {
    // FIXME: unpack to same object
    yrclient::HookableCommand h;
    auto r = client->run_one(h);
    decltype(h) aa;
    r.body().UnpackTo(&aa);
    return aa.result();
  }

  template <typename T>
  auto run(const T& cmd) {
    return client_utils::run(cmd, client.get());
  }
};

TEST_F(IServiceTest, HookingGetSetWorks) {
  // store initial flag value
  std::string key = "test_key";
  std::string flag1 = "0xdeadbeef";
  std::string flag2 = "0xbeefdead";
  auto value_eq = [&](std::string v) {
    yrclient::GetValue g;
    g.mutable_args()->set_key(key);
    auto r = run_one(g);
    ASSERT_EQ(r, v);
  };

  yrclient::StoreValue s;
  s.mutable_args()->set_key(key);
  s.mutable_args()->set_value(flag1);
  auto rrr = run_one(s);
  auto res0 = run_hookable();
  ASSERT_NE(res0.address_test_function(), 0);
  value_eq(flag1);
  yrclient::InstallHook ih;
  auto* ih_a = ih.mutable_args();
  ih_a->set_address(res0.address_test_function());
  ih_a->set_name("test_hook");
  ih_a->set_code_length(res0.code_size());
  auto res_ih_a = run(ih);
  value_eq(flag1);
  // install callback, which modifies the value (TODO: jit the callback)
  yrclient::AddCallback ac;
  ac.mutable_args()->set_hook_address(res0.address_test_function());
  ac.mutable_args()->set_callback_address(res0.address_test_callback());
  auto res_ac = run(ac);
  auto res1 = run_hookable();
  value_eq(flag2);
}

TEST_F(IServiceTest, BasicCommunicationWorks) {
  {
    std::vector<std::tuple<std::string, std::string>> datas = {
        {"test", "lol"}, {"aaa", "b"}, {"", ""}};

    // repeat few times
    for (int i = 0; i < 3; i++) {
      for (const auto& [name, value] : datas) {
        auto resp = client->send_command_old(name, value);
        std::cerr << to_json(resp) << std::endl;
        ASSERT_EQ(resp.code(), yrclient::RESPONSE_ERROR);
      }
    }
  }
}
