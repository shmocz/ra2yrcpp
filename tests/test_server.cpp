#include "config.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "gtest/gtest.h"
#include "server.hpp"
#include "util_string.hpp"
#include "utility/time.hpp"

#include <memory>
#include <thread>
#include <vector>

static const std::string T_MESSAGE = "CLIENT";
static const std::string T_KEY = "KEY";

using namespace yrclient;
using connection::Connection;

vecu8 on_receive(Connection* C, vecu8* bytes) {
  (void)C;
  (void)bytes;
  return to_bytes(T_KEY);
}

void on_send(Connection* C, vecu8* bytes) {
  (void)C;
  (void)bytes;
}

class ServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    S = get_server();
    S->start();
  }

  std::unique_ptr<server::Server> get_server() {
    return std::make_unique<server::Server>(
        max_clients, cfg::SERVER_PORT,
        server::Callbacks{&on_receive, &on_send, nullptr, nullptr},
        cfg::ACCEPT_TIMEOUT_MS);
  }

  void TearDown() override { S->shutdown(); }

  std::unique_ptr<server::Server> S;
  const unsigned int max_clients{4};
};

TEST_F(ServerTest, MultipleConnections) {
  std::vector<std::unique_ptr<Connection>> conns;
  auto conn_loop = [&](Connection* C) {
    auto msg = to_bytes(T_MESSAGE);
    C->send_bytes(msg);
    auto resp = C->read_bytes();
    if (resp.empty()) {
      throw std::runtime_error("connection closed");
    }
    auto q = to_string(resp);
    ASSERT_EQ(q, T_KEY);
  };

  auto loop_all = [&]() {
    for (auto& c : conns) {
      conn_loop(c.get());
    }
  };

  // Spawn max. number of connections
  for (auto i = 0u; i < max_clients; ++i) {
    conns.emplace_back(std::make_unique<Connection>(S->address(), S->port()));
  }

  // Loop conns
  loop_all();
  loop_all();

  // Spawn connection, this should fail
  {
    Connection cfail(S->address(), S->port());
    ASSERT_ANY_THROW(conn_loop(&cfail));
  }

  // Close one connection
  conns.pop_back();

  // Wait until ctx has been removed

  // Spawn connection, this should work
  {
    auto f = [this]() {
      std::unique_lock<std::mutex> lk(S->connections_mut);
      return S->connections().size();
    };
    while (f() == max_clients) {
      util::sleep_ms(10);
    }
    Connection cfail(S->address(), S->port());
    ASSERT_NO_THROW(conn_loop(&cfail));
  }
}

TEST_F(ServerTest, ServerConnectionAndMessagingWorks) {
  {
    {
      // Connect with client
      Connection C(S->address(), S->port());

      // Write messages and verify results
      auto msg = to_bytes(T_MESSAGE);
      auto i = C.send_bytes(msg);
      ASSERT_EQ(msg.size(), i);
      auto resp = C.read_bytes();
      auto q = to_string(resp);
      ASSERT_EQ(q, T_KEY);
      // Disconnect client
    }

    // Close server (leave scope)
  }
}
