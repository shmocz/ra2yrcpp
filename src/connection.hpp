#pragma once
#include "async_queue.hpp"
#include "errors.hpp"
#include "logging.hpp"
#include "network.hpp"
#include "types.h"
#include "utility/memtools.hpp"
#include "utility/scope_guard.hpp"
#include "utility/serialize.hpp"
#include "utility/sync.hpp"

#include <cstring>

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace connection {

using ReaderFn = std::function<vecu8(size_t)>;
using WriterFn = std::function<size_t(vecu8*)>;

enum State { NONE = 0, CONNECTING, OPEN, CLOSING, CLOSED };

// FIXME: duplicate code?
template <typename T>
T read_obj(ReaderFn reader) {
  auto buf = reader(sizeof(T));
  if (buf.size() != sizeof(T)) {
    throw std::runtime_error("Byte buffer too small");
  }
  return serialize::read_obj_le<T>(buf.data());
}

///
/// Interface for reading/writing in buffered chunks. Typical applications are
/// send()/recv() on a socket.
///
class ChunkReaderWriter {
 public:
  ChunkReaderWriter();
  u8* buf();
  unsigned int buflen() const;
  virtual vecu8 read_chunk(const size_t bytes) = 0;
  virtual size_t write_chunk(const vecu8& message) = 0;

 protected:
  u8 buf_[cfg::DEFAULT_BUFLEN];
};

///
/// Write bytes using the supplied writer.
/// @exception std::runtime_error on write failure
///
size_t write_bytes(const vecu8& bytes, ChunkReaderWriter* writer,
                   const size_t chunk_size = cfg::DEFAULT_BUFLEN);

///
/// Read bytes using the supplied reader.
/// @exception yrclient::timeout if reading timed out
/// @exception std::runtime_error on any other read failure
///
vecu8 read_bytes(ChunkReaderWriter* rw,
                 const size_t chunk_size = cfg::DEFAULT_BUFLEN);

class SocketIO : public ChunkReaderWriter {
 public:
  explicit SocketIO(network::socket_t* socket);
  ///
  /// Read bytes from socket.
  /// @param bytes number of bytes to read
  /// @exception yrclient::system_error if recv() fails on socket
  ///
  vecu8 read_chunk(const size_t bytes) override;

  ///
  /// Write bytes to socket.
  /// @param message bytes to write
  /// @exception yrclient::system_error if send() fails on socket
  ///
  size_t write_chunk(const vecu8& message) override;

 private:
  network::socket_t* socket_;
};

class Connection {
 public:
  Connection() = delete;
  /// Client connection
  Connection(std::string host, std::string port);
  /// Server (listening) connection
  explicit Connection(std::string port);
  explicit Connection(network::socket_t s);
  ~Connection();
  ///
  /// Send bytes and return number of bytes sent. TODO: use size_t
  /// @exception std::runtime_error on write failure
  ///
  int send_bytes(const vecu8& bytes);
  int send_bytes(const vecu8&& bytes);
  ///
  /// Read entire message from socket.
  /// FIXME: rename to read_message
  /// @exception std::runtime_error on read failure
  ///
  vecu8 read_bytes();
  network::socket_t socket();

 private:
  std::string host_;
  std::string port_;
  // network::addrinfo hints_;
  // addrinfo
  // TODO: remove?
  std::unique_ptr<void, void (*)(void*)> hints_;
  network::socket_t socket_{0};
};

class ClientConnection {
 public:
  ClientConnection(std::string host, std::string port);
  virtual ~ClientConnection() = default;
  virtual void connect() = 0;

  /// Send data with the underlying transport
  /// if it's a TCP connection, then encode size of the message at the beginning
  /// of data
  ///
  /// if WebSocket, then send blindly
  virtual bool send_data(const std::vector<u8>& bytes) = 0;
  bool send_data(std::vector<u8>&& bytes);

  ///
  /// Read a length-prefixed data message from connection
  ///
  /// @exception std::runtime_error on read failure
  virtual vecu8 read_data() = 0;
  virtual void stop();
  connection::State state();

 protected:
  std::string host;
  std::string port;
  std::mutex state_mut_;
  util::AtomicVariable<connection::State> state_;
};

class ClientTCPConnection : public ClientConnection {
 public:
  ClientTCPConnection(std::string host, std::string port);
  void connect() override;
  bool send_data(const std::vector<u8>& bytes) override;
  ~ClientTCPConnection() override;
  vecu8 read_data() override;

 private:
  std::unique_ptr<Connection> c_;
};

}  // namespace connection
