#pragma once
#include "protocol/protocol.hpp"

#include "connection.hpp"
#include "errors.hpp"
#include "util_string.hpp"
#include "utility/time.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace instrumentation_client {

using namespace std::chrono_literals;

class InstrumentationClient {
 public:
  InstrumentationClient(std::shared_ptr<connection::Connection> conn,
                        const std::chrono::milliseconds poll_timeout = 5000ms,
                        const std::chrono::milliseconds poll_rate = 250ms);
  InstrumentationClient(const std::string host, const std::string port,
                        const std::chrono::milliseconds poll_timeout = 5000ms,
                        const std::chrono::milliseconds poll_rate = 250ms);

  ///
  /// Send bytes and return number of bytes sent.
  /// @exception std::runtime_error on write failure
  ///
  size_t send_data(const vecu8& data);

  ///
  /// Send encoded message to server and read response back.
  /// @exception std::runtime_error on read/write failure.
  ///
  yrclient::Response send_message(const vecu8& data);
  /// Send protobuf message to server.
  yrclient::Response send_message(const google::protobuf::Message& M);

  yrclient::Response send_command_old(
      std::string name, std::string args,
      yrclient::CommandType type = yrclient::CLIENT_COMMAND);

  /// Send a command of given type to server and read response.
  yrclient::Response send_command(const google::protobuf::Message& cmd,
                                  yrclient::CommandType type);

  yrclient::Response poll();
  yrclient::NewCommandPollResult poll_until(
      const std::chrono::milliseconds timeout = 5000ms,
      const std::chrono::milliseconds rate = 250ms);
  yrclient::NewResult run_one(const google::protobuf::Message& M);
  std::string shutdown();

 private:
  std::shared_ptr<connection::Connection> conn_;
  const std::chrono::milliseconds poll_timeout_;
  const std::chrono::milliseconds poll_rate_;
};

}  // namespace instrumentation_client
