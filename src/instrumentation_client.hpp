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

class InstrumentationClient {
 public:
  // TODO: client should probs. own the connection
  InstrumentationClient(connection::Connection* conn,
                        const std::chrono::milliseconds poll_timeout = 5000ms,
                        const std::chrono::milliseconds poll_rate = 250ms);
  size_t send_data(const vecu8& data);

  yrclient::Response send_message(const vecu8& data);
  yrclient::Response send_message(const google::protobuf::Message& M);

  yrclient::Response send_command_old(
      std::string name, std::string args,
      yrclient::CommandType type = yrclient::CLIENT_COMMAND);

  yrclient::Response send_command(const google::protobuf::Message& cmd,
                                      yrclient::CommandType type);

  yrclient::Response poll();
  yrclient::NewCommandPollResult poll_until(
      const std::chrono::milliseconds timeout = 5000ms,
      const std::chrono::milliseconds rate = 250ms);
  yrclient::NewResult run_one(const google::protobuf::Message& M);
  std::string shutdown();

 private:
  connection::Connection* conn_;
  const std::chrono::milliseconds poll_timeout_;
  const std::chrono::milliseconds poll_rate_;
};

}  // namespace instrumentation_client
