#pragma once
#include "connection.hpp"
#include "protocol/protocol.hpp"
#include "util_string.hpp"
#include "utility/time.hpp"
#include <cassert>
#include <chrono>
#include <string>
#include <vector>

namespace instrumentation_client {

using namespace std::chrono_literals;

yrclient::CommandPollResult parse_poll(const yrclient::Response& R);

// type name(args...) { body }
// => type namespace::name(args...);
//     and:
//    type namespace::name(args..) { body }
//

std::vector<std::string> get_poll_results(const yrclient::Response& R);

class InstrumentationClient {
 public:
  // TODO: client should probs. own the connection
  InstrumentationClient(connection::Connection* conn,
                        const std::chrono::milliseconds poll_timeout = 5000ms,
                        const std::chrono::milliseconds poll_rate = 250ms);
  size_t send_data(const vecu8& data);

  yrclient::Response send_message(const vecu8& data);

  yrclient::Response send_command(
      std::string name, std::string args,
      yrclient::CommandType type = yrclient::CLIENT_COMMAND);

  yrclient::Response poll();
  yrclient::Response run_command(std::string name, std::string args = "");
  yrclient::CommandPollResult poll_until(
      const std::chrono::milliseconds timeout = 5000ms,
      const std::chrono::milliseconds rate = 250ms);
  std::string run_one(std::string name, std::string args = "");
  std::string run_one(std::string name, std::vector<std::string> args);
  std::string shutdown();

 private:
  connection::Connection* conn_;
  const std::chrono::milliseconds poll_timeout_;
  const std::chrono::milliseconds poll_rate_;
};

}  // namespace instrumentation_client
