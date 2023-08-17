#include "protocol/protocol.hpp"

#include "commands_builtin.hpp"
#include "common_multi.hpp"
#include "gtest/gtest.h"
#include "instrumentation_service.hpp"
#include "multi_client.hpp"
#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/core.pb.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using namespace multi_client;

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
  }

  void TearDown() override {
    ctx = nullptr;
    I = nullptr;
  }

  std::unique_ptr<InstrumentationService> I;
  std::unique_ptr<MultiClientTestContext> ctx;

  template <typename T>
  auto run_async(const T& cmd) {
    auto r = ctx->clients[0]->send_command(cmd);
    auto cmd_res = yrclient::from_any<ra2yrproto::CommandResult>(r.body());
    return yrclient::from_any<T>(cmd_res.result()).result();
  }
};

TEST_F(MultiClientTest, RunRegularCommand) {
  const unsigned count = 5u;

  ra2yrproto::commands::GetSystemState cmd;
  for (auto i = 0u; i < count; i++) {
    auto r = run_async<decltype(cmd)>(cmd);
    ASSERT_EQ(r.state().connections().size(), 2);
  }
}

TEST_F(MultiClientTest, RunCommandsAndVerify) {
  const int count = 10;
  const int val_size = 128;
  const std::string key = "tdata";
  for (int i = 0; i < count; i++) {
    std::string k1(val_size, static_cast<char>(i));
    ra2yrproto::commands::StoreValue sv;
    sv.mutable_args()->set_value(k1);
    sv.mutable_args()->set_key(key);
    auto r1 = run_async<decltype(sv)>(sv);

    ra2yrproto::commands::GetValue gv;
    gv.mutable_args()->set_key(key);
    auto r2 = run_async<decltype(gv)>(gv);
    ASSERT_EQ(r2.result(), k1);
  }
}
