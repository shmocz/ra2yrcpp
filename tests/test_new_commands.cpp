#include "google/protobuf/any.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "context.hpp"
#include "connection.hpp"
#include "instrumentation_service.hpp"
#include "instrumentation_client.hpp"
#include "is_context.hpp"
#include "protocol/protocol.hpp"
#include "util_string.hpp"
#include <cstdio>
#include <vector>
#include <iostream>
#include <memory>

using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;

yrclient::StoreValue get_storeval(std::string key, std::string val) {
  yrclient::StoreValue s;

  s.mutable_args()->set_key(key);
  s.mutable_args()->set_value(val);
  return s;
}

yrclient::GetValue get_getval(std::string key) {
  yrclient::GetValue s;

  s.mutable_args()->set_key(key);
  return s;
}

class NewCommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    I = std::make_unique<yrclient::InstrumentationService>(cfg::MAX_CLIENTS,
                                                           cfg::SERVER_PORT);
    auto& S = I->server();
    C_ = std::make_unique<connection::Connection>(S.address(), S.port());
    client = std::make_unique<InstrumentationClient>(C_.get(), 5000ms, 10ms);
    key = "key";
    val = "val";
  }

  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<connection::Connection> C_;
  std::unique_ptr<InstrumentationClient> client;
  std::string key;
  std::string val;
  // void TearDown() override {}
  auto get_storeval() { return ::get_storeval(key, val); }
  auto get_getval() { return ::get_getval(key); }

  void do_store(const std::string k, const std::string v) {
    auto s = ::get_storeval(k, v);
    auto r = client->run_one(s);
    std::string vv;
    decltype(s) aa;
    r.body().UnpackTo(&aa);
    ASSERT_EQ(aa.result(), v);
  }

  void do_get(const std::string k, const std::string v) {
    auto g = ::get_getval(k);
    auto r = client->run_one(g);
    std::string vv;
    decltype(g) aa;
    r.body().UnpackTo(&aa);
    ASSERT_EQ(aa.result(), v);
  }
};

TEST_F(NewCommandsTest, FetchAlot) {
  const size_t n_bytes = 1e6 * 10;
  const size_t count = 10;
  const size_t msg_size = n_bytes / count;
  const std::string key = "mega_key";
  for (auto i = 0u; i < count; i++) {
    // std::string k = "key_" + std::to_string(i);
    auto k = key;
    std::string v = std::string(msg_size, count);
    auto S = ::get_storeval(k, v);
    auto resp1 = client->run_one(S);
    do_get(k, v);
  }
}

TEST_F(NewCommandsTest, FetchOne) {
  auto s = get_storeval();
  auto res1 = client->run_one(s);
  auto g = get_getval();
  auto res2 = client->run_one(g);
  yrclient::GetValue v;
  res2.body().UnpackTo(&v);
  ASSERT_EQ(v.result(), val);
}

TEST_F(NewCommandsTest, BasicCommandTest) {
  {
    auto cmd_store = get_storeval();
    // schedule cmd, get ACK
    auto resp =
        client->send_command(cmd_store, yrclient::CLIENT_COMMAND_NEW);
    ASSERT_EQ(resp.code(), yrclient::OK);
    auto cmds = client->poll();
  }
}