#include "gtest/gtest.h"
#include "connection.hpp"
#include "config.hpp"
#include "debug_helpers.h"
#include "connection.hpp"
#include "util_string.hpp"
#include "server.hpp"
#include <unistd.h>
#include <vector>
#include <thread>

using namespace yrclient;

TEST(ServerTest, ServerConnectionAndMessagingWorks) {
  network::Init();
  const std::string message = "CLIENT";
  const std::string key = "KEY";
  {
    // Start server
    server::Server S(cfg::MAX_CLIENTS, cfg::SERVER_PORT,
                     {[&key](connection::Connection* C, vecu8* bytes) -> vecu8 {
                        return to_bytes(key);
                      },
                      [](connection::Connection* C, vecu8* bytes) -> void {}});
    {
      // Connect with client
      connection::Connection C(S.address(), S.port());

      // Write messages and verify results
      auto msg = to_bytes(message);
      C.send_bytes(msg);
      auto resp = C.read_bytes();
      auto q = to_string(resp);
      ASSERT_EQ(q, key);
      // Disconnect client
    }

    // Close server (leave scope)
  }
}
