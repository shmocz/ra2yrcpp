#pragma once

#include "types.h"
#include "utility/sync.hpp"

#include <mutex>
#include <string>

namespace ra2yrcpp::connection {

enum State { NONE = 0, CONNECTING, OPEN, CLOSING, CLOSED };

class ClientConnection {
 public:
  ClientConnection(std::string host, std::string port);
  virtual ~ClientConnection() = default;
  virtual void connect() = 0;

  ///
  /// Send data with the underlying transport.
  ///
  /// @exception std::runtime_error on write failure
  virtual void send_data(const vecu8& bytes) = 0;
  void send_data(vecu8&& bytes);

  ///
  /// Read a length-prefixed data message from connection. A previous call to
  /// send_data must've occurred, or no data will be available to read.
  ///
  /// @exception std::runtime_error on read failure
  /// @exception ra2yrcpp::protocol_error
  virtual vecu8 read_data() = 0;
  virtual void stop();
  util::AtomicVariable<ra2yrcpp::connection::State>& state();

 protected:
  std::string host;
  std::string port;
  std::mutex state_mut_;
  util::AtomicVariable<connection::State> state_;
};

}  // namespace ra2yrcpp::connection
