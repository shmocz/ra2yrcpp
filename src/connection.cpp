#include "connection.hpp"

#include "errors.hpp"
#include "logging.hpp"
#include "utility/memtools.hpp"
#include "utility/scope_guard.hpp"

#include <fmt/core.h>

#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#elif __linux__
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#endif

using namespace connection;

ChunkReaderWriter::ChunkReaderWriter() {
  std::memset(&buf_[0], 0, sizeof(buf_));
}

u8* ChunkReaderWriter::buf() { return buf_; }

unsigned int ChunkReaderWriter::buflen() const { return sizeof(buf_); }

SocketIO::SocketIO(network::socket_t* socket) : socket_(socket) {}

vecu8 SocketIO::read_chunk(const size_t bytes) {
  size_t to_read = std::min(static_cast<size_t>(buflen()), bytes);
  ssize_t res;
#ifdef DEBUG_SOCKETIO
  dprintf("recv sock={},bytes={}", *socket_, bytes);
#endif
  if ((res = network::recv(*socket_, buf_, bytes, 0)) < 0) {
    auto e = network::get_last_network_error();
    if (e == network::ETIMEOUT) {
      throw yrclient::timeout("read_chunk(): timeout");
    }
    dprintf("neterr={}", e);
    throw yrclient::system_error(std::string("recv() ") + std::to_string(res),
                                 e);
  }
  if (res == 0) {
    throw yrclient::system_error("read_chunk() connection closed");
  }
  return vecu8(buf(), buf() + to_read);
}

size_t SocketIO::write_chunk(const vecu8& message) {
  size_t to_send = std::min(static_cast<size_t>(buflen()), message.size());
  ssize_t res;
#ifdef DEBUG_SOCKETIO
  dprintf("writing, sock={},bytes={}", *socket_, to_send);
#endif
  if ((res = network::send(*socket_, message.data(), to_send, 0)) < 0) {
    throw yrclient::system_error("send()");
  }
  if (res == 0) {
    throw yrclient::system_error("write_chunk(): connection closed");
  }
  return to_send;
}

vecu8 connection::read_bytes(ChunkReaderWriter* rw, const size_t chunk_size) {
  // FIXME: dedicated method
  auto f = [&rw](const size_t l) {
    try {
      return rw->read_chunk(l);
    } catch (const yrclient::system_error& e) {
      throw std::runtime_error(
          e.what());  // FIXME: just throw the original exception
    }
  };
  u32 length = 0U;
  try {
    length = read_obj<u32>(f);
  } catch (const std::runtime_error& e) {
    if (std::string(e.what()).find("connection closed") != std::string::npos) {
      return {};
    }
    throw;
  }

  if (length > cfg::MAX_MESSAGE_LENGTH) {
    throw std::runtime_error(fmt::format("Too large message: {} (max={})",
                                         length, cfg::MAX_MESSAGE_LENGTH));
  }
  vecu8 res(length, 0);
  size_t bytes = length;
  auto it = res.begin();
  while (bytes > 0) {
    size_t bytes_chunk = std::min(bytes, chunk_size);
    try {
      vecu8 chunk = f(bytes_chunk);
      if (bytes_chunk != chunk.size()) {
        throw std::runtime_error("Chunk size mismatch");
      }
      std::copy(chunk.begin(), chunk.end(), it);
      bytes -= chunk.size();
      it += chunk.size();
    } catch (const yrclient::timeout& e) {
      throw std::runtime_error("Broken connection");
    }
  }
  return res;
}

size_t connection::write_bytes(const vecu8& bytes, ChunkReaderWriter* writer,
                               const size_t chunk_size) {
  auto f = [&writer](const vecu8& v) {
    try {
      return writer->write_chunk(v);
    } catch (const yrclient::system_error& e) {
      throw std::runtime_error(e.what());
    }
  };

  size_t c_sent = 0u;
  u32 sz = bytes.size();
  if (sz > cfg::MAX_MESSAGE_LENGTH) {
    throw std::runtime_error(fmt::format("Too large message: {} (max={})", sz,
                                         cfg::MAX_MESSAGE_LENGTH));
  }

  // Write length
  vecu8 ssz(sizeof(u32), 0);
  std::copy(reinterpret_cast<char*>(&sz),
            (reinterpret_cast<char*>(&sz)) + sizeof(sz), ssz.begin());
  f(ssz);

  // Write rest of the data
  auto* dat = bytes.data();
  for (auto i = 0u; i < bytes.size(); i += chunk_size) {
    vecu8 msg(dat + i, dat + std::min(i + chunk_size, bytes.size()));
    size_t count = f(msg);
    c_sent += count;
  }

  if (c_sent != bytes.size()) {
    throw std::runtime_error("Failed to write bytes");
  }
  return c_sent;
}

// FIXME
// TODO: retries
Connection::Connection(std::string host, std::string port)
    : host_(host), port_(port), hints_(utility::make_uptr<addrinfo>()) {
  // FIXME: set default hints in all overloads
  memset(hints_.get(), 0, sizeof(addrinfo));
  auto* h = reinterpret_cast<addrinfo*>(hints_.get());
  h->ai_family = AF_INET;
  h->ai_protocol = IPPROTO_TCP;
  h->ai_socktype = SOCK_STREAM;
  socket_ = network::BAD_SOCKET;
  {
    addrinfo* result;
    network::getaddrinfo(host, port, h, &result);
    utility::scope_guard guard = [&result]() { network::freeaddrinfo(result); };
    // Connect until success/fail
    for (auto* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
      socket_ = network::socket(ptr);
      int res = network::connect(socket_, ptr->ai_addr,
                                 static_cast<int>(ptr->ai_addrlen));
      if (res == -1) {
        network::closesocket(socket_);
        socket_ = network::BAD_SOCKET;
        continue;
      }
      break;
    }
  }
  dprintf("init_sock={}", socket_);
}

Connection::Connection(std::string port)
    : port_(port), hints_(utility::make_uptr<addrinfo>()) {
  memset(hints_.get(), 0, sizeof(addrinfo));
  auto* h = reinterpret_cast<addrinfo*>(hints_.get());
  h->ai_family = AF_INET;
  h->ai_protocol = IPPROTO_TCP;
  h->ai_socktype = SOCK_STREAM;
  h->ai_flags = AI_PASSIVE;

  auto result = network::agetaddrinfo("", port, h);
  socket_ = network::socket(result.get());
  int s = 1;

  setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&s), sizeof(int));
#ifdef __linux__
  if (ioctl(socket_, FIONBIO, reinterpret_cast<char*>(&s)) < 0) {
    throw yrclient::system_error("ioctl() failed");
  }
#endif
  if (network::bind(socket_, result->ai_addr, result->ai_addrlen)) {
    throw yrclient::system_error("bind()");
  }

  if (network::listen(socket_, SOMAXCONN)) {
    throw yrclient::system_error("listen()");
  }
}

Connection::Connection(network::socket_t s)
    : socket_(s), hints_(utility::make_uptr<addrinfo>()) {
  memset(hints_.get(), 0, sizeof(addrinfo));
}

Connection::~Connection() {
  try {
    dprintf("closing {}", socket_);
    network::shutdown(socket_, 0);
    network::closesocket(socket_);
  } catch (const yrclient::system_error& e) {
    dprintf("closesocket() failed, something's messed up");
  }
}

// TODO: pass by pointer
int Connection::send_bytes(const vecu8& bytes) {
  connection::SocketIO S(&socket_);
  return write_bytes(bytes, &S);
}

int Connection::send_bytes(const vecu8&& bytes) { return send_bytes(bytes); }

vecu8 Connection::read_bytes() {
  connection::SocketIO S(&socket_);
  return ::read_bytes(&S);
}

network::socket_t Connection::socket() { return socket_; }

ClientConnection::ClientConnection(std::string host, std::string port)
    : host(host), port(port), state_(State::NONE) {}

bool ClientConnection::send_data(vecu8&& bytes) { return send_data(bytes); }

ClientTCPConnection::ClientTCPConnection(std::string host, std::string port)
    : ClientConnection(host, port) {}

ClientTCPConnection::~ClientTCPConnection() {}

void ClientTCPConnection::connect() {
  c_ = std::make_unique<Connection>(host, port);
  // FIXME: dont hardcode
  network::set_io_timeout(c_->socket(), 10000);
  state_.store(State::OPEN);
}

// FIXME: make parent method
bool ClientTCPConnection::send_data(const vecu8& bytes) {
  if (state_ != State::OPEN) {
    throw std::runtime_error("Connection not open");
  }
  c_->send_bytes(bytes);
  return true;
}

// FIXME: make parent method
vecu8 ClientTCPConnection::read_data() {
  if (state_ != State::OPEN) {
    throw std::runtime_error("Connection not open");
  }

  return c_->read_bytes();
}

void ClientConnection::stop() {}

util::AtomicVariable<connection::State>& ClientConnection::state() {
  return state_;
}
