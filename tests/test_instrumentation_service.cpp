#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/core.pb.h"

#include "asio_utils.hpp"
#include "client_utils.hpp"
#include "command/command_manager.hpp"
#include "commands_builtin.hpp"
#include "config.hpp"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "util_proto.hpp"
#include "util_string.hpp"
#include "utility/sync.hpp"
#include "websocket_connection.hpp"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <websocketpp/common/asio.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <cstddef>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using namespace ra2yrcpp::test_util;

namespace lib = websocketpp::lib;
namespace pb = google::protobuf;

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

namespace command = ra2yrcpp::command;

class CommandTest : public ::testing::Test {
 protected:
  void SetUp() {
    M = std::make_unique<command::CommandManager<cmd_d_t>>();
    M->add_command("test", [](auto*) { dprintf("this is a test"); });
    M->start();
  }

  void TearDown() {
    M->shutdown();
    M = nullptr;
  }

  auto flush(const u64 qid) { return M->flush_results(qid, 5.0s); }

  auto make_cmd(const u64 qid) {
    return M->enqueue_command(M->make_command("test", cmd_d_t(), qid));
  }

 public:
  using cmd_d_t = pb::Any;
  std::unique_ptr<command::CommandManager<cmd_d_t>> M;
};

TEST_F(CommandTest, BasicTest) {
  constexpr u64 queue_id = 1;

  (void)M->execute_create_queue(queue_id);
  {
    (void)make_cmd(queue_id);
    auto C_res = flush(queue_id);
    ASSERT_EQ(C_res.size(), 1);
  }

  (void)M->execute_destroy_queue(queue_id);
  {
    for (u64 i = 0; i < 25; i++) {
      auto C = make_cmd(i);
      C->result_code().wait_pred(
          [](command::ResultCode v) { return v != command::ResultCode::NONE; });
      ASSERT_THROW({ (void)flush(i); }, std::out_of_range);
    }
  }

  (void)M->execute_create_queue(queue_id, cfg::RESULT_QUEUE_SIZE);
  {
    (void)make_cmd(queue_id);
    auto C_res = flush(queue_id);
    ASSERT_EQ(C_res.size(), 1);
  }

  // Check that result queue size bounds work
  {
    for (unsigned i = 0U; i < cfg::RESULT_QUEUE_SIZE + 5U; i++) {
      auto C = make_cmd(queue_id);
      C->result_code().wait_pred(
          [](command::ResultCode v) { return v != command::ResultCode::NONE; });
    }
    auto C_res = flush(queue_id);
    ASSERT_EQ(C_res.size(), cfg::RESULT_QUEUE_SIZE);
  }
}

TEST_F(CommandTest, ComplexTest) {
  std::vector<std::future<void>> tasks;
  util::AtomicVariable<bool> queues_ready(false);
  util::AtomicVariable<bool> dupe_tasks_ready(false);
  std::atomic<int> count = 0;
  constexpr std::size_t queue_count = 10;
  constexpr std::size_t dupe_tasks = 20;

  auto main_task = [&](const u64 id) {
    // Create queue
    (void)M->execute_create_queue(id);
    count++;
    if (count == 10) {
      queues_ready.store(true);
    } else {
      queues_ready.wait(true);
    }

    dupe_tasks_ready.wait(true);

    const int tasks_per_queue = 5;
    // Run some tasks
    for (std::size_t i = 0; i < queue_count; i++) {
      for (int j = 0; j < tasks_per_queue; j++) {
        (void)make_cmd(id);
      }

      for (int j = tasks_per_queue; j > 0;) {
        auto C_res = flush(id);
        ASSERT_GT(C_res.size(), 0);
        j -= static_cast<int>(C_res.size());
        ASSERT_GE(j, 0);
      }
    }
  };

  auto task_dupe_queue = [&](const u64 id) {
    queues_ready.wait(true);
    (void)M->execute_create_queue(id % queue_count);
  };

  for (std::size_t i = 0; i < queue_count; i++) {
    tasks.emplace_back(std::async(std::launch::async, main_task, i));
  }

  for (std::size_t i = 0; i < dupe_tasks; i++) {
    tasks.emplace_back(std::async(std::launch::async, task_dupe_queue, i));
  }

  dupe_tasks_ready.store(true);

  for (u64 i = 0; i < tasks.size(); i++) {
    tasks.at(i).wait();
  }
}
