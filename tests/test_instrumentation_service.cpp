#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/core.pb.h"

#include "asio_utils.hpp"
#include "client_utils.hpp"
#include "commands_builtin.hpp"
#include "config.hpp"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "util_proto.hpp"
#include "util_string.hpp"
#include "websocket_connection.hpp"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <websocketpp/common/asio.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <regex>
#include <string>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using namespace ra2yrcpp::test_util;

namespace lib = websocketpp::lib;

using instrumentation_client::InstrumentationClient;
using yrclient::InstrumentationService;

class InstrumentationServiceTest : public ::testing::Test {
  using conn_t = ra2yrcpp::connection::ClientWebsocketConnection;

 protected:
  void SetUp() override;
  void TearDown() override;

  virtual void init() = 0;
  std::unique_ptr<ra2yrcpp::asio_utils::IOService> srv;
  std::shared_ptr<conn_t> conn;
  std::unique_ptr<InstrumentationService> I;
  std::unique_ptr<InstrumentationClient> client;
  std::unique_ptr<client_utils::CommandSender> cs;
};

void InstrumentationServiceTest::SetUp() {
  InstrumentationService::Options O = yrclient::default_options;

  srv = std::make_unique<ra2yrcpp::asio_utils::IOService>();
  I = std::unique_ptr<InstrumentationService>(InstrumentationService::create(
      O, yrclient::commands_builtin::get_commands(), nullptr));

  conn = std::make_shared<conn_t>(O.server.host, std::to_string(O.server.port),
                                  srv.get());
  conn->connect();
  client = std::make_unique<InstrumentationClient>(conn);
  cs = std::make_unique<client_utils::CommandSender>([&](const auto& msg) {
    return client_utils::run_one(msg, client.get());
  });

  init();
}

void InstrumentationServiceTest::TearDown() {
  cs = nullptr;
  client = nullptr;
  conn = nullptr;
  I = nullptr;
}

class IServiceTest : public InstrumentationServiceTest {
 protected:
  void init() override {}
};

TEST_F(IServiceTest, HookingGetSetWorks) {
#ifdef RA2YRCPP_64
  GTEST_SKIP();
#endif
  // store initial flag value
  std::string key = "test_key";
  std::string flag1 = "0xdeadbeef";
  std::string flag2 = "0xbeefdead";
  auto value_eq = [&](std::string v) {
    auto r = cs->run(GetValue::create({key, ""}));
    ASSERT_EQ(r.value(), v);
  };

  auto h = HookableCommand::create({});
  (void)cs->run(StoreValue::create({key, flag1}));
  auto res0 = cs->run(h);
  ASSERT_NE(res0.address_test_function(), 0);
  value_eq(flag1);
  {
    ra2yrproto::HookEntry E;
    E.set_address(res0.address_test_function());
    E.set_name("test_hook");
    E.set_code_length(res0.code_size());
    std::vector<ra2yrproto::HookEntry> V;
    V.push_back(E);
    auto res_ih_a = cs->run(CreateHooks::create({true, V}));
    value_eq(flag1);
  }
  // install callback, which modifies the value (TODO: jit the callback)
  auto ac = AddCallback::create(
      {res0.address_test_function(), res0.address_test_callback()});
  (void)cs->run(ac);
  (void)cs->run(h);
  value_eq(flag2);
}

class NewCommandsTest : public InstrumentationServiceTest {
 protected:
  void init() override {
    key = "key";
    val = "val";
  }

  std::string key;
  std::string val;
};

TEST_F(NewCommandsTest, FetchManySizes) {
  const std::size_t count = 20;
  const std::size_t max_size = (cfg::MAX_MESSAGE_LENGTH / 1000u);
  const std::string key = "mega_key";

  for (auto i = 0u; i < count; i++) {
    const std::size_t sz = (i + 1) * (max_size / count);
    std::string v = std::string(sz, 'X');

    {
      auto r = cs->run(StoreValue::create({key, v}));
      ASSERT_EQ(r.value(), v);
    }
    {
      auto r = cs->run(GetValue::create({key, ""}));
      ASSERT_EQ(r.value(), v);
    }
  }
}

// TODO: strange bug with larger messages (>1M) causing messages not being
// received properly causing incorrect size to be read
TEST_F(NewCommandsTest, FetchAlot) {
  const std::size_t count = 10;
  const std::size_t msg_size = cfg::MAX_MESSAGE_LENGTH / 1000u;
  const std::string key = "mega_key";
  std::string v = std::string(msg_size, 'X');
  for (auto i = 0u; i < count; i++) {
    (void)cs->run(StoreValue::create({key, v}));
    auto aa = cs->run(GetValue::create({key, ""}));
    ASSERT_EQ(aa.value(), v);
  }
}

TEST_F(NewCommandsTest, FetchOne) {
  // cppcheck-suppress unreadVariable
  auto res1 = cs->run(StoreValue::create({key, val}));
  auto res2 = cs->run(GetValue::create({key, ""}));
  ASSERT_EQ(res2.value(), val);
}

TEST_F(NewCommandsTest, BasicCommandTest) {
  auto cmd_store = StoreValue::create({key, val});
  // schedule cmd, get ACK
  auto resp = client->send_command(cmd_store, ra2yrproto::CLIENT_COMMAND);
  ASSERT_EQ(resp.code(), ra2yrproto::ResponseCode::OK);
  auto cmds = client->poll_blocking(5.0s);
  ASSERT_EQ(cmds.result().results().size(), 1);
}

TEST_F(IServiceTest, TestHTTPRequest) {
#if 0
  B("ra2yrproto.commands.GetSystemState");
  Use these commands to re - generate the string message.auto* mm =
      yrclient::create_command_message(&B, "");
  auto cc =
      yrclient::create_command(*mm, ra2yrproto::CommandType::CLIENT_COMMAND);
  cc.set_blocking(true);
  auto cmd_json = ra2yrcpp::protocol::to_json(cc);
#endif
  // FIXME(shmocz): adding this delay fixes the unpack error. Race condition?
  std::string cmd_json2 =
      "{\"commandType\":\"CLIENT_COMMAND\",\"command\":{\"@type\":\"type."
      "googleapis.com/"
      "ra2yrproto.commands.GetSystemState\"},\"blocking\":true}";
  // dprintf("json={}", cmd_json);
  // dprintf("json={}", cmd_json2);

  std::string msg = fmt::format(
      "POST / HTTP/1.1\r\nHost: 127.0.0.1:14525\r\nAccept: "
      "*/*\r\nContent-Type: application/json\r\nContent-Length: "
      "{}\r\n\r\n{}",
      cmd_json2.size(), cmd_json2);

  auto S0 = std::make_shared<ra2yrcpp::asio_utils::IOService>();

  const int count = 32;
  for (int i = 0; i < count; i++) {
    ra2yrcpp::asio_utils::AsioSocket A(S0);

    A.connect(I->opts().server.host, std::to_string(I->opts().server.port));
    auto sz = A.write(msg);
    ASSERT_EQ(sz, msg.size());

    lib::asio::error_code ec;

    // Read the whole message
    std::string rsp = A.read();
    ASSERT_GT(rsp.size(), 1);

    // Get content-length
    std::regex re("Content-Length: (\\d+)");
    std::smatch match;

    ASSERT_TRUE(std::regex_search(rsp, match, re));
    ASSERT_GT(match.size(), 1);
    auto bbody = yrclient::to_bytes(rsp.substr(rsp.find("\r\n\r\n") + 4));
    ra2yrproto::Response R;
    ASSERT_TRUE(ra2yrcpp::protocol::from_json(bbody, &R));
  }
  iprintf("exit test");
}
