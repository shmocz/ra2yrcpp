#include "gtest/gtest.h"
#include "config.hpp"
#include "debug_helpers.h"
#include "protocol/protocol.hpp"
#include "connection.hpp"
#include "util_string.hpp"
#include "instrumentation_service.hpp"
#include <unistd.h>
#include <vector>
#include <thread>
#include <cassert>
#include <memory>

class InstrumentationClient {
 public:
  // TODO: client should probs. own the connection
  InstrumentationClient(connection::Connection& conn) : conn_(conn) {}
  yrclient::Response send_command(std::string name, std::string args) {
    yrclient::Command C;
    C.set_command_type(yrclient::CLIENT_COMMAND);
    auto* CC = C.mutable_client_command();
    CC->set_name(name);
    CC->set_args(args);
    auto data = yrclient::to_vecu8(C);
    assert(!data.empty());
    assert(conn_.send_bytes(data) == data.size());
    auto resp = conn_.read_bytes();
    yrclient::Response R;
    R.ParseFromArray(resp.data(), resp.size());
    return R;
  }

 private:
  connection::Connection& conn_;
};

using namespace yrclient;

class IServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    I = std::make_unique<yrclient::InstrumentationService>(cfg::MAX_CLIENTS,
                                                           cfg::SERVER_PORT);
  }

  std::unique_ptr<yrclient::InstrumentationService> I;
  // void TearDown() override {}
};

#if 0
TEST(InstrumentationServiceTest, BasicHookingAndCommandsWork) {
  network::Init();
  yrclient::InstrumentationService I(cfg::MAX_CLIENTS, cfg::SERVER_PORT);
  {

  }
}
#endif

TEST_F(IServiceTest, BasicCommunicationWorks) {
  {
    auto& S = I->server();
    // Connect with client
    connection::Connection C(S.address(), S.port());
    InstrumentationClient IC(C);

    std::vector<std::tuple<std::string, std::string>> datas = {
        {"test", "lol"}, {"aaa", "b"}, {"", ""}};

    // repeat few times
    for (int i = 0; i < 3; i++) {
      for (const auto& [name, value] : datas) {
        auto resp = IC.send_command(name, value);
        std::cerr << to_json(resp) << std::endl;
        ASSERT_EQ(resp.code(), yrclient::RESPONSE_ERROR);
      }
    }
  }
}
