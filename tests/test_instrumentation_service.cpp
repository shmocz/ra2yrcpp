#include "gtest/gtest.h"
#include "config.hpp"
#include "debug_helpers.h"
#include "protocol/protocol.hpp"
#include "connection.hpp"
#include "util_string.hpp"
#include "utility/time.hpp"
#include "instrumentation_service.hpp"
#include "instrumentation_client.hpp"
#include <unistd.h>
#include <chrono>
#include <vector>
#include <thread>
#include <cassert>
#include <memory>

using namespace yrclient;
using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;

struct hookable_command_res {
  std::string addr_test, code_size, addr_test_cb;
};

// TODO: const-correctness
hookable_command_res run_hookable(InstrumentationClient* C) {
  hookable_command_res res;

  auto s = C->run_one("hookable_command");
  auto r = split_string(s);
  return hookable_command_res{r[0], r[1], r[2]};
}

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
};

TEST_F(IServiceTest, BasicCommandsWork) {
  {
    GTEST_SKIP();
    //  Execute test command and verify it's result
    std::vector<std::string> first;
    for (int i = 0; i < 15; i++) {
      auto resp = client->run_command("test_command", "lol");
      CommandPollResult P;
      resp.body().UnpackTo(&P);
      if (i == 0) {
        for (auto& c : P.results()) {
          first.push_back(c.data());
        }
        ASSERT_EQ(first.size(), 1);
      }
      ASSERT_EQ(P.results().size(), first.size());
    }
  }
}

TEST_F(IServiceTest, HookingGetSetWorks) {
  // store initial flag value
  std::string key = "test_key";
  std::string flag1 = "0xdeadbeef";
  std::string flag2 = "0xbeefdead";
  auto value_eq = [&](std::string v) {
    ASSERT_EQ(client->run_one("get_value", key), v);
  };

  client->run_one("store_value", {key, flag1});
  auto res0 = run_hookable(client.get());
  value_eq(flag1);
  client->run_one("install_hook",
                  {"test_hook", res0.addr_test, res0.code_size});
  value_eq(flag1);

  // install callback, which modifies the value (TODO: jit the callback)
  client->run_one("add_callback", {res0.addr_test, res0.addr_test_cb});
  auto res1 = run_hookable(client.get());

  value_eq(flag2);
}

TEST_F(IServiceTest, BasicCommunicationWorks) {
  {
    std::vector<std::tuple<std::string, std::string>> datas = {
        {"test", "lol"}, {"aaa", "b"}, {"", ""}};

    // repeat few times
    for (int i = 0; i < 3; i++) {
      for (const auto& [name, value] : datas) {
        auto resp = client->send_command(name, value);
        std::cerr << to_json(resp) << std::endl;
        ASSERT_EQ(resp.code(), yrclient::RESPONSE_ERROR);
      }
    }
  }
}
