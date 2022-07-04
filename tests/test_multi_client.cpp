#include "protocol/protocol.hpp"

#include "client_utils.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "debug_helpers.h"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "multi_client.hpp"
#include "utility/time.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <thread>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;
using namespace multi_client;

class MultiClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    I = std::make_unique<yrclient::InstrumentationService>(cfg::MAX_CLIENTS,
                                                           cfg::SERVER_PORT);
    auto& S = I->server();
    client = std::make_unique<AutoPollClient>(S.address(), S.port());
  }

  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<AutoPollClient> client;

  template <typename T>
  auto run(const T& cmd) {
    return client_utils::run(cmd,
                             client.get()->get_client(ClientType::COMMAND));
  }

  template <typename T>
  auto run_async(const T& cmd) {
    auto r = client->send_command(cmd);
    DPRINTF("body=%s\n", to_json(r.body()).c_str());
    auto cmd_res = yrclient::from_any<yrclient::CommandResult>(r.body());
    return yrclient::from_any<T>(cmd_res.result()).result();
  }
};

TEST_F(MultiClientTest, RunRegularCommand) {
  const unsigned count = 5u;
  yrclient::commands::HookableCommand cmd;
  for (auto i = 0u; i < count; i++) {
    DPRINTF("number=%d\n", i);
    auto res0 = run_async<decltype(cmd)>(cmd);
    DPRINTF("res=%s\n", to_json(res0).c_str());
  }
}

TEST_F(MultiClientTest, RunCommandsAndVerify) {
  yrclient::commands::HookableCommand cmd;
  auto r = run_async<decltype(cmd)>(cmd);
  const u32 addr = r.address_test_callback();
  ASSERT_GT(r.address_test_callback(), 0);
  const int count = 10;
  for (int i = 0; i < count; i++) {
    r = run_async<decltype(cmd)>(cmd);
    ASSERT_EQ(r.address_test_callback(), addr);
  }
}
