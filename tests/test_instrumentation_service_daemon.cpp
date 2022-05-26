#include "gtest/gtest.h"
#include "context.hpp"
#include "connection.hpp"
#include "client_utils.hpp"
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
  template <typename T>
  auto run(const T& cmd) {
    return client_utils::run(cmd, client.get());
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
    yrclient::StoreValue s;
    s.mutable_args()->set_key(key);
    s.mutable_args()->set_value(flag1);
    auto res0 = client_utils::run(s, client.get());
    yrclient::GetValue g;
    g.mutable_args()->set_key(key);

    auto res1 = client_utils::run(g, client.get());
    ASSERT_EQ(res1, flag1);
    auto r = client->shutdown();
    ctx.join();
  }
}
