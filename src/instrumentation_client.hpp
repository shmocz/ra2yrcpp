#pragma once
#include "connection.hpp"
#include "ra2yrproto/core.pb.h"
#include "types.h"

#include <cstddef>
#include <memory>
#include <string>

namespace connection {
class ClientConnection;
}

namespace google {
namespace protobuf {
class Message;
}
}  // namespace google

namespace instrumentation_client {

class InstrumentationClient {
 public:
  explicit InstrumentationClient(
      std::shared_ptr<connection::ClientConnection> conn);

  ///
  /// Send bytes and return number of bytes sent.
  /// @exception std::runtime_error on write failure
  ///
  size_t send_data(const vecu8& data);

  ///
  /// Send encoded message to server and read response back.
  /// @exception std::runtime_error on read/write failure.
  /// @exception yrclient::protocol_error on message serialization failure.
  ///
  ra2yrproto::Response send_message(const vecu8& data);
  /// Convert message to bytes and send it to server.
  ra2yrproto::Response send_message(const google::protobuf::Message& M);

  ///
  /// Send a command of given type to server and read response. This can block
  /// if there's nothing to be read.
  ///
  /// @exception std::runtime_error on read/write failure.
  ///
  ra2yrproto::Response send_command(const google::protobuf::Message& cmd,
                                    ra2yrproto::CommandType type);
  /// @exception std::system_error for internal server error
  ra2yrproto::PollResults poll_blocking(const duration_t timeout,
                                        const u64 queue_id = (u64)-1);
  connection::ClientConnection* connection();

 private:
  std::shared_ptr<connection::ClientConnection> conn_;
};

}  // namespace instrumentation_client
