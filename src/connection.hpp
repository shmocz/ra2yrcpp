#pragma once
#include "config.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "network.hpp"
#include "types.h"
#include "utility/scope_guard.hpp"

#include <cstring>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace connection {

using ReaderFn = std::function<vecu8(size_t)>;
using WriterFn = std::function<size_t(vecu8*)>;

// Deserialize object from byte buffer.
template <typename T>
T deserialize_obj(T* dst, const u8* src, bool little_endian = false) {
  u8* p = reinterpret_cast<u8*>(dst);
  auto b = src;
  auto e = src + sizeof(T);
  if (little_endian) {
    std::copy_backward(b, e, p);
  } else {
    std::copy(b, e, p);
  }
  return *dst;
}

template <typename T>
T read_obj(ReaderFn reader) {
  T res = 0;
  auto buf = reader(sizeof(T));
  if (buf.size() != sizeof(T)) {
    throw std::runtime_error("Byte buffer too small");
  }
  return deserialize_obj<T>(&res, &buf[0]);
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
  ///
  /// Read entire message from socket.
  /// @exception std::runtime_error on read failure
  ///
  vecu8 read_bytes();
  network::socket_t socket();

 private:
  std::string host_;
  std::string port_;
  network::addrinfo hints_;
  network::socket_t socket_{0};
};
}  // namespace connection
