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
#include <cstdlib>
#include <exception>
#include <future>
#include <memory>
#include <thread>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;
using namespace multi_client;
using ra2yrcpp::websocket_server::IOService;

using ra2yrcpp::tests::MultiClientTestContext;

static void add_builtin_commands(yrclient::InstrumentationService* I) {
  for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
    I->add_command(name, fn);
  }
}

yrclient::InstrumentationService* make_is(
    yrclient::InstrumentationService::IServiceOptions O) {
  auto* I = new yrclient::InstrumentationService(O, nullptr);
  add_builtin_commands(I);
  return I;
}

class ISStressTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    yrclient::InstrumentationService::IServiceOptions opts{
        cfg::MAX_CLIENTS, cfg::SERVER_PORT, cfg::WEBSOCKET_PROXY_PORT,
        "0.0.0.0", true};

    I = std::unique_ptr<yrclient::InstrumentationService>(make_is(opts));
    ctx = std::make_unique<MultiClientTestContext>();
  }

  void TearDown() override {
    ctx = nullptr;
    I = nullptr;
  }

  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<MultiClientTestContext> ctx;
};

TEST_F(ISStressTest, DISABLED_ManyConnections) {
  yrclient::InstrumentationService::IServiceOptions opts{
      cfg::MAX_CLIENTS, cfg::SERVER_PORT, cfg::WEBSOCKET_PROXY_PORT,
      "127.0.0.1", true};
  AutoPollClient::Options aopts{
      opts.host, std::to_string(opts.ws_port), 1000ms,
      250ms,     CONNECTION_TYPE::WEBSOCKET,   nullptr};

  ra2yrproto::commands::GetSystemState cmd;
  // TODO: make the failed connections fail more quickly
  for (int i = 0; i < 1; i++) {
    try {
      ctx->create_client(aopts);
      auto& c = ctx->clients.back();
      // TODO: issue the initial command internally
      auto r = c->send_command(cmd);
    } catch (const std::exception& e) {
      eprintf("connection failed: {}", e.what());
    }
  }

  auto& c = ctx->clients[0];
  // spam  storevalue
  size_t total_bytes = 1e9;
  size_t chunk_size = 1e4 * 5;
  ra2yrproto::commands::StoreValue sv;
  std::string k1(chunk_size, 'A');
  sv.mutable_args()->set_value(k1);
  sv.mutable_args()->set_key("tdata");

  for (size_t chunks = total_bytes / chunk_size; chunks > 0; chunks--) {
    auto r = c->send_command(sv);
  }

  // remove a client
#if 0
  try {
    ctx->clients.erase(ctx->clients.begin());
  } catch (const std::exception& e) {
    eprintf("erase fail");
  }
#endif
}
