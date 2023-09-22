#include "ra2yrproto/commands_builtin.pb.h"

#include "commands_builtin.hpp"
#include "common_multi.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "multi_client.hpp"
#include "util_proto.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using namespace multi_client;

using ra2yrcpp::InstrumentationService;
using ra2yrcpp::tests::MultiClientTestContext;
using namespace ra2yrcpp::test_util;

class ISStressTest : public ::testing::Test {
 protected:
  void SetUp() override {
    I = std::unique_ptr<InstrumentationService>(InstrumentationService::create(
        ra2yrcpp::default_options, ra2yrcpp::commands_builtin::get_commands(),
        nullptr));
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
  for (int i = 0; i < 1; i++) {
    try {
      ctx->create_client(multi_client::default_options);
    } catch (const std::exception& e) {
      eprintf("connection failed: {}", e.what());
    }
  }

  size_t total_bytes = 1e9;
  size_t chunk_size = 1e4 * 5;

  // spam  storevalue
  std::string k1(chunk_size, 'A');
  auto sv = StoreValue::create({k1, "tdata"});
  for (size_t chunks = total_bytes / chunk_size; chunks > 0; chunks--) {
    auto r = ctx->clients[0]->send_command(sv);
  }
}
