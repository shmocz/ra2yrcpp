#include "connection.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "network.hpp"
#include "utility/scope_guard.hpp"
#include <algorithm>
#include <string>
#include <stdexcept>

using namespace connection;

u8* ChunkReaderWriter::buf() { return buf_; }
unsigned int ChunkReaderWriter::buflen() const { return sizeof(buf_); }

SocketIO::SocketIO(network::socket_t& socket) : socket_(socket) {}
vecu8 SocketIO::read_chunk(const size_t bytes) {
  size_t to_read = std::min(buflen(), bytes);
  ssize_t res;
  DPRINTF("recv sock=%d,bytes=%d\n", socket_, bytes);
  if ((res = network::recv(socket_, buf_, bytes, 0)) < 0) {
    throw yrclient::system_error("recv()");
  }
  if (res == 0) {
    throw std::runtime_error("Connection closed");
  }
  return vecu8(buf(), buf() + to_read);
}

size_t SocketIO::write_chunk(vecu8& message) {
  size_t to_send = std::min(buflen(), message.size());
  ssize_t res;
  DPRINTF("writing, sock=%d,bytes=%d\n", socket_, message.size());
  if ((res = network::send(socket_, &message[0], to_send, 0)) < 0) {
    throw yrclient::system_error("send()");
  }
  if (res == 0) {
    throw std::runtime_error("Connection closed");
  }
  return to_send;
}

vecu8 connection::read_bytes(ChunkReaderWriter& rw, const size_t chunk_size) {
  // FIXME: dedicated method
  auto f = [&rw](const size_t l) { return rw.read_chunk(l); };
  u32 length = read_obj<u32>(f);
  // Read message body
  DPRINTF("Reading a message of size %d\n", length);
  if (length > cfg::MAX_MESSAGE_LENGTH) {
    throw std::runtime_error("Too large message");
  }
  vecu8 res(length, 0);
  size_t bytes = length;
  auto it = res.begin();
  while (bytes > 0) {
    size_t bytes_chunk = std::min(bytes, chunk_size);
    vecu8 chunk = rw.read_chunk(bytes_chunk);
    if (bytes_chunk != chunk.size()) {
      throw std::runtime_error("Chunk size mismatch");
    }
    std::copy(chunk.begin(), chunk.end(), it);
    bytes -= chunk.size();
    it += chunk.size();
  }
  if (bytes != 0) {
    throw std::runtime_error("Remaining bytes should be zero");
  }
  return res;
}

size_t connection::write_bytes(vecu8& bytes, ChunkReaderWriter& writer,
                               const size_t chunk_size) {
  size_t c_sent = 0u;
  size_t sz = bytes.size();
  // Write length
  vecu8 ssz(sizeof(u32), 0);
  std::copy((char*)&sz, ((char*)&sz) + sizeof(sz), ssz.begin());
  writer.write_chunk(ssz);
  // Write rest of the data
  for (auto it = bytes.begin(); it < bytes.end(); it += chunk_size) {
    vecu8 msg(it, std::min(bytes.end(), it + chunk_size));
    size_t count = writer.write_chunk(msg);
    c_sent += count;
  }
  if (c_sent != bytes.size()) {
    throw std::runtime_error("Failed to write bytes");
  }
  return c_sent;
}

// TODO: retries
Connection::Connection(std::string host, std::string port)
    : host_(host), port_(port) {
  // FIXME: set default hints in all overloads
  memset(&hints_, 0, sizeof(hints_));
  hints_.ai_family = AF_UNSPEC;
  hints_.ai_socktype = SOCK_STREAM;
  hints_.ai_protocol = IPPROTO_TCP;
  {
    network::addrinfo* result;
    network::getaddrinfo(host.c_str(), port.c_str(), &hints_, &result);
    utility::scope_guard guard = [&result]() { network::freeaddrinfo(result); };
    int res;
    // Connect until success/fail
    for (auto* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
      socket_ = network::socket(ptr);
      res = network::connect(socket_, ptr->ai_addr, (int)ptr->ai_addrlen);
      if (res == -1) {
        network::closesocket(socket_);
        socket_ = network::BAD_SOCKET;
        continue;
      }
      break;
    }
  }
}

Connection::Connection(std::string port) : port_(port) {
  memset(&hints_, 0, sizeof(hints_));
  hints_.ai_family = AF_INET;
  hints_.ai_socktype = SOCK_STREAM;
  hints_.ai_protocol = IPPROTO_TCP;
  hints_.ai_flags = AI_PASSIVE;
  network::addrinfo* result;
  network::getaddrinfo("", port_.c_str(), &hints_, &result);
  socket_ = network::socket(result);
  if (network::bind(socket_, result->ai_addr, result->ai_addrlen)) {
    throw yrclient::system_error("bind()");
  }
  if (network::listen(socket_, SOMAXCONN)) {
    throw yrclient::system_error("listen()");
  }
}

Connection::Connection(network::socket_t s) : socket_(s) {}
Connection::~Connection() { network::closesocket(socket_); }
// TODO: pass by pointer
int Connection::send_bytes(vecu8& bytes) {
  connection::SocketIO S(socket_);
  return write_bytes(bytes, S);
}
vecu8 Connection::read_bytes() {
  connection::SocketIO S(socket_);
  return ::read_bytes(S);
}
network::socket_t Connection::socket() { return socket_; }
