#include "protocol/protocol.hpp"

#include "asio_utils.hpp"
#include "client_utils.hpp"
#include "command/command.hpp"
#include "commands_builtin.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "util_string.hpp"
#include "websocket_connection.hpp"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <websocketpp/common/asio.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace google {
namespace protobuf {
class Message;
}
}  // namespace google

using namespace yrclient;
using namespace std::chrono_literals;

namespace lib = websocketpp::lib;

using instrumentation_client::InstrumentationClient;

class InstrumentationServiceTest : public ::testing::Test {
  using conn_t = ra2yrcpp::connection::ClientWebsocketConnection;

 protected:
  void SetUp() override;
  void TearDown() override;

  virtual void init() = 0;
  std::unique_ptr<ra2yrcpp::asio_utils::IOService> srv;
  std::shared_ptr<conn_t> conn;
  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<InstrumentationClient> client;

  auto run_one(const google::protobuf::Message& M) {
    return client_utils::run_one(M, client.get());
  }
};

void InstrumentationServiceTest::SetUp() {
  yrclient::InstrumentationService::IServiceOptions O{
      cfg::MAX_CLIENTS, cfg::SERVER_PORT, cfg::SERVER_ADDRESS, true};

  std::map<std::string, command::Command::handler_t> cmds;

  for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
    cmds[name] = fn;
  }

  srv = std::make_unique<ra2yrcpp::asio_utils::IOService>();
  I = std::unique_ptr<yrclient::InstrumentationService>(
      yrclient::InstrumentationService::create(O, &cmds, nullptr));

  conn = std::make_shared<conn_t>(O.host, std::to_string(O.port), srv.get());
  conn->connect();
  client = std::make_unique<InstrumentationClient>(conn);
  init();
}

void InstrumentationServiceTest::TearDown() {
  client = nullptr;
  conn = nullptr;
  I = nullptr;
}

class IServiceTest : public InstrumentationServiceTest {
 protected:
  template <typename T>
  auto run(const T& cmd) {
    auto r = run_one(cmd);
    return yrclient::from_any<T>(r.result()).result();
  }

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
    ra2yrproto::commands::GetValue g;
    g.mutable_args()->set_key(key);
    auto r = run(g);
    ASSERT_EQ(r.result(), v);
  };

  ra2yrproto::commands::StoreValue s;
  ra2yrproto::commands::HookableCommand h;
  s.mutable_args()->set_key(key);
  s.mutable_args()->set_value(flag1);
  auto rrr = run(s);
  auto res0 = run(h);
  ASSERT_NE(res0.address_test_function(), 0);
  value_eq(flag1);
  {
    ra2yrproto::commands::CreateHooks C;
    auto* A = C.mutable_args();
    A->set_no_suspend_threads(true);
    auto* H = A->add_hooks();
    H->set_address(res0.address_test_function());
    H->set_name("test_hook");
    H->set_code_length(res0.code_size());
    auto res_ih_a = run(C);
    value_eq(flag1);
  }
  // install callback, which modifies the value (TODO: jit the callback)
  ra2yrproto::commands::AddCallback ac;
  ac.mutable_args()->set_hook_address(res0.address_test_function());
  ac.mutable_args()->set_callback_address(res0.address_test_callback());
  (void)run(ac);
  (void)run(h);
  value_eq(flag2);
}

ra2yrproto::commands::StoreValue get_storeval(std::string key,
                                              std::string val) {
  ra2yrproto::commands::StoreValue s;

  s.mutable_args()->set_key(key);
  s.mutable_args()->set_value(val);
  return s;
}

ra2yrproto::commands::GetValue get_getval(std::string key) {
  ra2yrproto::commands::GetValue s;

  s.mutable_args()->set_key(key);
  return s;
}

class NewCommandsTest : public InstrumentationServiceTest {
 protected:
  void init() override {
    key = "key";
    val = "val";
  }

  std::string key;
  std::string val;

  auto get_storeval() { return ::get_storeval(key, val); }

  auto get_getval() { return ::get_getval(key); }

  void do_get(const std::string k, const std::string v) {
    auto g = ::get_getval(k);
    auto r = run_one(g);
    decltype(g) aa;
    r.result().UnpackTo(&aa);
    ASSERT_EQ(aa.result().result(), v);
  }

  void do_run(google::protobuf::Message* M) {
    auto r = run_one(*M);
    r.result().UnpackTo(M);
  }
};

TEST_F(NewCommandsTest, FetchManySizes) {
  const std::size_t count = 20;
  const std::size_t max_size = (cfg::MAX_MESSAGE_LENGTH / 1000u);
  const std::string key = "mega_key";
  for (auto i = 0u; i < count; i++) {
    const std::size_t sz = (i + 1) * (max_size / count);
    std::string v = std::string(sz, 'X');

    // std::string k = "key_" + std::to_string(i);
    {
      auto S = ::get_storeval(key, v);
      do_run(&S);
      ASSERT_EQ(S.result().result(), v);
    }
    {
      auto G = ::get_getval(key);
      do_run(&G);
      // auto r = run_one(G);
      // r.result().UnpackTo(&G);
      ASSERT_EQ(G.result().result(), v);
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
    auto S = ::get_storeval(key, v);
    (void)run_one(S);
    do_get(key, v);
  }
}

TEST_F(NewCommandsTest, FetchOne) {
  auto s = get_storeval();
  // cppcheck-suppress unreadVariable
  auto res1 = run_one(s);
  auto g = get_getval();
  auto res2 = run_one(g);
  ra2yrproto::commands::GetValue v;
  res2.result().UnpackTo(&v);
  ASSERT_EQ(v.result().result(), val);
}

TEST_F(NewCommandsTest, BasicCommandTest) {
  auto cmd_store = get_storeval();
  // schedule cmd, get ACK
  auto resp = client->send_command(cmd_store, ra2yrproto::CLIENT_COMMAND);
  ASSERT_EQ(resp.code(), yrclient::RESPONSE_OK);
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
  auto cmd_json = yrclient::to_json(cc);
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

    A.connect(I->opts().host, std::to_string(I->opts().port));
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
    ASSERT_TRUE(yrclient::from_json(bbody, &R));
  }
  iprintf("exit test");
}
