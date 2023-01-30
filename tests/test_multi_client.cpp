#include "protocol/protocol.hpp"

#include "client_utils.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "logging.hpp"
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
    I = std::unique_ptr<yrclient::InstrumentationService>(
        is_context::make_is({cfg::MAX_CLIENTS, cfg::SERVER_PORT, 0U, ""}));
    auto& S = I->server();
    client =
        std::make_unique<AutoPollClient>(S.address(), S.port(), 100ms, 5000ms);
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
    dprintf("body={}\n", to_json(r.body()).c_str());
    auto cmd_res = yrclient::from_any<ra2yrproto::CommandResult>(r.body());
    return yrclient::from_any<T>(cmd_res.result()).result();
  }
};

TEST_F(MultiClientTest, RunRegularCommand) {
  const unsigned count = 5u;
  ra2yrproto::commands::HookableCommand cmd;
  for (auto i = 0u; i < count; i++) {
    (void)run_async<decltype(cmd)>(cmd);
  }
}

TEST_F(MultiClientTest, RunCommandsAndVerify) {
  ra2yrproto::commands::HookableCommand cmd;
  auto r = run_async<decltype(cmd)>(cmd);
  const u32 addr = r.address_test_callback();
  ASSERT_GT(r.address_test_callback(), 0);
  const int count = 10;
  for (int i = 0; i < count; i++) {
    r = run_async<decltype(cmd)>(cmd);
    ASSERT_EQ(r.address_test_callback(), addr);
  }
}
