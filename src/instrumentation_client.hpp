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
  explicit InstrumentationClient(
      std::shared_ptr<connection::ClientConnection> conn,
      const std::chrono::milliseconds poll_timeout = 5000ms);

  ///
  /// Send bytes and return number of bytes sent.
  /// @exception std::runtime_error on write failure
  ///
  size_t send_data(const vecu8& data);

  ///
  /// Send encoded message to server and read response back.
  /// @exception std::runtime_error on read/write failure.
  ///
  ra2yrproto::Response send_message(const vecu8& data);
  /// Send protobuf message to server.
  ra2yrproto::Response send_message(const google::protobuf::Message& M);

  ra2yrproto::Response send_command_old(
      std::string name, std::string args,
      ra2yrproto::CommandType type = ra2yrproto::CLIENT_COMMAND_OLD);

  /// Send a command of given type to server and read response.
  ra2yrproto::Response send_command(const google::protobuf::Message& cmd,
                                    ra2yrproto::CommandType type);

  ra2yrproto::Response poll(const std::chrono::milliseconds timeout = 0ms);
  ra2yrproto::PollResults poll_blocking(
      const std::chrono::milliseconds timeout = 5000ms,
      const u64 queue_id = (u64)-1);
  ra2yrproto::PollResults poll_until(
      const std::chrono::milliseconds timeout = 5000ms);
  ///
  /// Run single command on the backend and poll result immediately back.
  ///
  /// @param M command encoded into protobuf message
  /// @result of the command
  /// @exception yrclient::timeout if result isn't available within specified
  /// time interval.
  ///
  ra2yrproto::CommandResult run_one(const google::protobuf::Message& M);
  std::string shutdown();
  connection::ClientConnection* connection();

 private:
  std::shared_ptr<connection::ClientConnection> conn_;
  const std::chrono::milliseconds poll_timeout_;
};

}  // namespace instrumentation_client
