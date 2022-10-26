#include "client_utils.hpp"
#include "connection.hpp"
#include "context.hpp"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"

#include <iostream>
#include <memory>
#include <vector>

using namespace yrclient;
using context::Context;
using instrumentation_client::InstrumentationClient;

class IClient {
 public:
  explicit IClient(InstrumentationService* I) {
    const auto& S = I->server();
    client = std::make_unique<InstrumentationClient>(S.address(), S.port());
  }

  template <typename T>
  auto run(const T& cmd) {
    return client_utils::run(cmd, client.get());
  }

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
    Context ctx;
    is_context::make_is_ctx(&ctx);
    IClient C(reinterpret_cast<InstrumentationService*>(ctx.data()));
    auto& client = C.client;
    yrclient::commands::StoreValue s;
    s.mutable_args()->set_key(key);
    s.mutable_args()->set_value(flag1);
    auto res0 = client_utils::run(s, client.get());
    std::cerr << res0 << std::endl;
    yrclient::commands::GetValue g;
    g.mutable_args()->set_key(key);

    auto res1 = client_utils::run(g, client.get());
    ASSERT_EQ(res1, flag1);
    auto r = client->shutdown();
    std::cerr << r << std::endl;
    ctx.join();
  }
}
