#include "protocol/protocol.hpp"

#include "client_utils.hpp"
#include "commands_builtin.hpp"
#include "common_multi.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "multi_client.hpp"
#include "utility/time.hpp"
#include "websocket_server.hpp"

#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <thread>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;
using namespace multi_client;

using ra2yrcpp::tests::MultiClientTestContext;

class MultiClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    yrclient::InstrumentationService::IServiceOptions opts{
        cfg::MAX_CLIENTS, cfg::SERVER_PORT, cfg::WEBSOCKET_PROXY_PORT,
        "127.0.0.1", true};
    AutoPollClient::Options aopts{
        opts.host, std::to_string(opts.ws_port), 1.0s,
        0.25s,     CONNECTION_TYPE::WEBSOCKET,   nullptr};

    std::map<std::string, command::Command::handler_t> cmds;

    for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
      cmds[name] = fn;
    }

    I = std::unique_ptr<yrclient::InstrumentationService>(
        yrclient::InstrumentationService::create(opts, &cmds, nullptr));
    ctx = std::make_unique<MultiClientTestContext>();
    ctx->create_client(aopts);
  }

  void TearDown() override {
    ctx = nullptr;
    I = nullptr;
    network::Deinit();
  }

  // FIXME: need to ensure websocketproxy is properly setup before starting
  // conns
  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<MultiClientTestContext> ctx;

  template <typename T>
  auto run_async(const T& cmd) {
    auto r = ctx->clients[0]->send_command(cmd);
    dprintf("body={}\n", to_json(r.body()).c_str());
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
