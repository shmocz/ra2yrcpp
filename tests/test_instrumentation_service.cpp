#include "gtest/gtest.h"
#include "config.hpp"
#include "debug_helpers.h"
#include "protocol/protocol.hpp"
#include "connection.hpp"
#include "util_string.hpp"
#include "utility/time.hpp"
#include "instrumentation_service.hpp"
#include <unistd.h>
#include <chrono>
#include <vector>
#include <thread>
#include <cassert>
#include <memory>

using namespace yrclient;
using namespace std::chrono_literals;

CommandPollResult parse_poll(yrclient::Response& R) {
  CommandPollResult P;
  R.body().UnpackTo(&P);
  return P;
}

std::vector<std::string> get_poll_results(yrclient::Response& R) {
  std::vector<std::string> res;
  auto P = parse_poll(R);
  for (auto& c : P.results()) {
    res.push_back(c.data());
  }
  return res;
}

// TODO: put to separate file
class InstrumentationClient {
 public:
  // TODO: client should probs. own the connection
  InstrumentationClient(connection::Connection& conn) : conn_(conn) {}
  size_t send_data(const vecu8& data) {
    size_t sent = conn_.send_bytes(data);
    assert(sent == data.size());
    return sent;
  }
  yrclient::Response send_message(const vecu8& data) {
    (void)send_data(data);
    auto resp = conn_.read_bytes();
    yrclient::Response R;
    R.ParseFromArray(resp.data(), resp.size());
    return R;
  }
  yrclient::Response send_command(
      std::string name, std::string args,
      yrclient::CommandType type = yrclient::CLIENT_COMMAND) {
    yrclient::Command C;
    C.set_command_type(type);
    if (type == yrclient::CLIENT_COMMAND) {
      auto* CC = C.mutable_client_command();
      CC->set_name(name);
      CC->set_args(args);
    }
    auto data = yrclient::to_vecu8(C);
    assert(!data.empty());
    auto R = send_message(data);
    return R;
  }

  yrclient::Response poll() {
    yrclient::Command C;
    C.set_command_type(yrclient::POLL);
    auto R = send_command("", "", yrclient::POLL);
    return R;
  }

  yrclient::Response run_command(std::string name, std::string args = "") {
    auto r_ack = send_command(name, args);
    auto r_body = poll();
    return r_body;
  }

  yrclient::CommandPollResult poll_until(
      const std::chrono::milliseconds timeout = 5000ms,
      const std::chrono::milliseconds rate = 250ms) {
    CommandPollResult P;
    std::chrono::milliseconds c = 0ms;
    auto deadline = util::current_time() + timeout;
    while (P.results().size() < 1 && util::current_time() < deadline) {
      auto R = poll();
      P = parse_poll(R);
      util::sleep_ms(rate);
    }
    return P;
  }

  std::string run_one(std::string name, std::string args = "") {
    auto r_ack = send_command(name, args);
    auto res = poll_until();
    auto err = [&](std::string args) {
      throw std::runtime_error(name + " " + args);
    };
    if (res.results().size() < 1) {
      err("No results in queue");
    }
    if (res.results().size() > 1) {
      err("Excess entries in result queue");
    }
    auto res0 = res.results()[0];
    if (res0.result_code() == RESPONSE_ERROR) {
      err("Command failed");
    }
    DPRINTF("name=%s, args=%s, result=%s\n", name.c_str(), args.c_str(),
            res0.data().c_str());
    return res0.data();
  }

  std::string run_one(std::string name, std::vector<std::string> args) {
    return run_one(name, join_string(args));
  }

 private:
  connection::Connection& conn_;
};

struct hookable_command_res {
  std::string addr_test, code_size, addr_test_cb;
};

hookable_command_res run_hookable(InstrumentationClient& C) {
  hookable_command_res res;

  auto s = C.run_one("hookable_command");
  auto r = split_string(s);
  return hookable_command_res{r[0], r[1], r[2]};
}

class IServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    I = std::make_unique<yrclient::InstrumentationService>(cfg::MAX_CLIENTS,
                                                           cfg::SERVER_PORT);
    auto& S = I->server();
    C_ = std::make_unique<connection::Connection>(S.address(), S.port());
    client = std::make_unique<InstrumentationClient>(*C_);
  }

  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<connection::Connection> C_;
  std::unique_ptr<InstrumentationClient> client;
  // void TearDown() override {}
};

TEST_F(IServiceTest, BasicCommandsWork) {
  {
    //  Execute test command and verify it's result
    std::vector<std::string> first;
    for (int i = 0; i < 15; i++) {
      auto resp = client->run_command("test_command", "lol");
      CommandPollResult P;
      resp.body().UnpackTo(&P);
      if (i == 0) {
        for (auto& c : P.results()) {
          first.push_back(c.data());
        }
        ASSERT_EQ(first.size(), 1);
      }
      ASSERT_EQ(P.results().size(), first.size());
    }
  }
}

TEST_F(IServiceTest, HookingGetSetWorks) {
  // store initial flag value
  std::string key = "test_key";
  std::string flag1 = "0xdeadbeef";
  std::string flag2 = "0xbeefdead";
  auto value_eq = [&](std::string v) {
    ASSERT_EQ(client->run_one("get_value", key), v);
  };

  client->run_one("store_value", {key, flag1});
  auto res0 = run_hookable(*client);
  value_eq(flag1);
  client->run_one("install_hook",
                  {"test_hook", res0.addr_test, res0.code_size});
  value_eq(flag1);

  // install callback, which modifies the value (TODO: jit the callback)
  client->run_one("add_callback", {res0.addr_test, res0.addr_test_cb});
  auto res1 = run_hookable(*client);

  value_eq(flag2);
}

// TODO: old stuff.. remove
TEST_F(IServiceTest, BasicHookingAndCommandsWork) {
  {
    GTEST_SKIP();
    struct test_res {
      std::string addr_flag, addr_test, code_size, value_flag, addr_test_cb;
    };
    auto parse_res = [](std::vector<std::string> r) {
      return test_res{r[0], r[1], r[2], r[3], r[4]};
    };
    const std::string flag0 = "0xbeefdead";
    const std::string flag1 = "0xdeadbeef";

    // Execute test command and verify it's result
    auto resp = client->run_command("test_command", "lol");
    auto res = get_poll_results(resp);
    auto res0 = res[0];
    ASSERT_EQ(res.size(), 1);
    auto toks = split_string(res0);
    auto tres0 = parse_res(toks);
    ASSERT_EQ(tres0.value_flag, flag0);

    // Install hook at command's entry point
    auto resp_hook = client->run_command(
        "install_hook", "test " + tres0.addr_test + " " + tres0.code_size);

    // Execute test command and verify that it still returns original value
    auto resp_t1 = client->run_command("test_command", "lol");
    res = get_poll_results(resp_t1);
    ASSERT_EQ(res.size(), 2);
    res0 = res[1];
    auto tres1 = parse_res(split_string(res0));
    ASSERT_EQ(tres1.value_flag, flag0);

    // Install test callback  TODO: inject machine code directly)
    auto resp_t2 = client->run_command(
        "add_callback", tres0.addr_test + " " + tres0.addr_test_cb);
    // Execute test command and verify the hook is working
    auto resp_t3 = client->run_command("test_command", "lol");
    res = get_poll_results(resp_t3);
    ASSERT_EQ(res.size(), 1);
    res0 = res[0];
    auto tres2 = parse_res(split_string(res0));
    ASSERT_EQ(tres2.value_flag, flag1);
  }
}

TEST_F(IServiceTest, BasicCommunicationWorks) {
  {
    std::vector<std::tuple<std::string, std::string>> datas = {
        {"test", "lol"}, {"aaa", "b"}, {"", ""}};

    // repeat few times
    for (int i = 0; i < 3; i++) {
      for (const auto& [name, value] : datas) {
        auto resp = client->send_command(name, value);
        std::cerr << to_json(resp) << std::endl;
        ASSERT_EQ(resp.code(), yrclient::RESPONSE_ERROR);
      }
    }
  }
}
