#include "gtest/gtest.h"
#include "context.hpp"
#include "connection.hpp"
#include "instrumentation_service.hpp"
#include "instrumentation_client.hpp"
#include "is_context.hpp"
#include <vector>
#include <memory>

using namespace yrclient;
using context::Context;
using instrumentation_client::InstrumentationClient;

class IClient {
 public:
  explicit IClient(InstrumentationService* I) {
    auto& S = I->server();
    C_ = std::make_unique<connection::Connection>(S.address(), S.port());
    client = std::make_unique<InstrumentationClient>(C_.get());
  }
  std::unique_ptr<connection::Connection> C_;
  std::unique_ptr<InstrumentationClient> client;
};

class IServiceDaemonTest : public ::testing::Test {
 protected:
  void SetUp() override { network::Init(); }
};

TEST_F(IServiceDaemonTest, BasicSetup) {
  {
    std::string key = "test_key";
    std::string flag1 = "0xdeadbeef";
    std::string flag2 = "0xbeefdead";
    Context ctx;
    is_context::make_is_ctx(&ctx);
    IClient C(reinterpret_cast<InstrumentationService*>(ctx.data()));
    auto& client = C.client;
    client->run_one("store_value", {key, flag1});
    ASSERT_EQ(client->run_one("get_value", key), flag1);
    auto r = client->shutdown();
    ctx.join();
  }
}
