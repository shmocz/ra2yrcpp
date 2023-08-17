#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/core.pb.h"

#include "client_utils.hpp"
#include "commands_builtin.hpp"
#include "common_multi.hpp"
#include "gtest/gtest.h"
#include "instrumentation_service.hpp"
#include "multi_client.hpp"
#include "protocol/helpers.hpp"
#include "util_proto.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using namespace multi_client;
using namespace ra2yrcpp::test_util;

using ra2yrcpp::tests::MultiClientTestContext;
using yrclient::InstrumentationService;

class MultiClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    InstrumentationService::Options opts = yrclient::default_options;

    I = std::unique_ptr<InstrumentationService>(InstrumentationService::create(
        opts, yrclient::commands_builtin::get_commands(), nullptr));
    ctx = std::make_unique<MultiClientTestContext>();
    ctx->create_client(multi_client::default_options);
    cs = std::make_unique<client_utils::CommandSender>([&](auto& msg) {
      auto r = ctx->clients[0]->send_command(msg);
      return ra2yrcpp::protocol::from_any<ra2yrproto::CommandResult>(r.body());
    });
  }

  void TearDown() override {
    cs = nullptr;
    ctx = nullptr;
    I = nullptr;
  }

  std::unique_ptr<InstrumentationService> I;
  std::unique_ptr<MultiClientTestContext> ctx;
  std::unique_ptr<client_utils::CommandSender> cs;
};

TEST_F(MultiClientTest, RunRegularCommand) {
  const unsigned count = 5u;

  ra2yrproto::commands::GetSystemState cmd;
  for (auto i = 0u; i < count; i++) {
    auto r = cs->run(cmd);
    ASSERT_EQ(r.state().connections().size(), 2);
  }
}

TEST_F(MultiClientTest, RunCommandsAndVerify) {
  const int count = 10;
  const int val_size = 128;
  const std::string key = "tdata";
  for (int i = 0; i < count; i++) {
    std::string k1(val_size, static_cast<char>(i));
    auto sv = StoreValue::create({key, k1});
    auto r1 = cs->run(sv);

    auto gv = GetValue::create({key, ""});
    auto r2 = cs->run(gv);
    ASSERT_EQ(r2.value(), k1);
  }
}
