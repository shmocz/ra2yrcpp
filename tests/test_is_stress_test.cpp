#include "protocol/protocol.hpp"

#include "commands_builtin.hpp"
#include "common_multi.hpp"
#include "config.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "multi_client.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace multi_client;

using ra2yrcpp::tests::MultiClientTestContext;
using yrclient::InstrumentationService;

static void add_builtin_commands(InstrumentationService* I) {
  for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
    I->add_command(name, fn);
  }
}

InstrumentationService* make_is(InstrumentationService::Options O) {
  auto* I = new InstrumentationService(O, nullptr);
  add_builtin_commands(I);
  return I;
}

// FIXME: dedupe code
class ISStressTest : public ::testing::Test {
 protected:
  void SetUp() override {
    InstrumentationService::Options opts = yrclient::default_options;

    I = std::unique_ptr<InstrumentationService>(make_is(opts));
    ctx = std::make_unique<MultiClientTestContext>();
  }

  void TearDown() override {
    ctx = nullptr;
    I = nullptr;
  }

  std::unique_ptr<InstrumentationService> I;
  std::unique_ptr<MultiClientTestContext> ctx;
};

TEST_F(ISStressTest, DISABLED_ManyConnections) {
  auto& opts = I->opts();
  AutoPollClient::Options aopts{
      opts.server.host, std::to_string(opts.server.port),
      cfg::POLL_RESULTS_TIMEOUT, cfg::COMMAND_ACK_TIMEOUT, nullptr};

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
