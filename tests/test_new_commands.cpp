#include "protocol/protocol.hpp"

#include "common.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "context.hpp"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "util_string.hpp"

#include <cstdio>

#include <iostream>
#include <memory>
#include <vector>

using namespace std::chrono_literals;

yrclient::commands::StoreValue get_storeval(std::string key, std::string val) {
  yrclient::commands::StoreValue s;

  s.mutable_args()->set_key(key);
  s.mutable_args()->set_value(val);
  return s;
}

yrclient::commands::GetValue get_getval(std::string key) {
  yrclient::commands::GetValue s;

  s.mutable_args()->set_key(key);
  return s;
}

class NewCommandsTest : public ra2yrcpp::tests::InstrumentationServiceTest {
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
    auto r = client->run_one(g);
    decltype(g) aa;
    r.result().UnpackTo(&aa);
    ASSERT_EQ(aa.result(), v);
  }

  void do_run(google::protobuf::Message* M) {
    auto r = client->run_one(*M);
    r.result().UnpackTo(M);
  }
};

TEST_F(NewCommandsTest, FetchManySizes) {
  const size_t count = 20;
  const size_t max_size = (cfg::MAX_MESSAGE_LENGTH / 1000u);
  const std::string key = "mega_key";
  for (auto i = 0u; i < count; i++) {
    const size_t sz = (i + 1) * (max_size / count);
    std::string v = std::string(sz, 'X');
    dprintf("msg {}/{}, size={}", i, count, v.size());
    // std::string k = "key_" + std::to_string(i);
    {
      auto S = ::get_storeval(key, v);
      do_run(&S);
      ASSERT_EQ(S.result(), v);
    }
    {
      auto G = ::get_getval(key);
      do_run(&G);
      // auto r = client->run_one(G);
      // r.result().UnpackTo(&G);
      ASSERT_EQ(G.result(), v);
    }
  }
}

// TODO: strange bug with larger messages (>1M) causing messages not being
// received properly causing incorrect size to be read
TEST_F(NewCommandsTest, FetchAlot) {
  const size_t count = 10;
  const size_t msg_size = cfg::MAX_MESSAGE_LENGTH / 1000u;
  // const size_t msg_size = 5000u;
  const std::string key = "mega_key";
  std::string v = std::string(msg_size, 'X');
  for (auto i = 0u; i < count; i++) {
    dprintf("msg {}/{}, size={}", i, count, v.size());
    // std::string k = "key_" + std::to_string(i);
    auto S = ::get_storeval(key, v);
    (void)client->run_one(S);
    do_get(key, v);
  }
}

TEST_F(NewCommandsTest, FetchOne) {
  auto s = get_storeval();
  // cppcheck-suppress unreadVariable
  auto res1 = client->run_one(s);
  auto g = get_getval();
  auto res2 = client->run_one(g);
  yrclient::commands::GetValue v;
  res2.result().UnpackTo(&v);
  ASSERT_EQ(v.result(), val);
}

TEST_F(NewCommandsTest, BasicCommandTest) {
  {
    auto cmd_store = get_storeval();
    // schedule cmd, get ACK
    auto resp = client->send_command(cmd_store, yrclient::CLIENT_COMMAND);
    ASSERT_EQ(resp.code(), yrclient::OK);
    // cppcheck-suppress unreadVariable
    auto cmds = client->poll();
  }
}
