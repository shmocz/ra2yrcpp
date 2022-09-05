#include "connection.hpp"

using namespace connection;

ChunkReaderWriter::ChunkReaderWriter() {
  std::memset(&buf_[0], 0, sizeof(buf_));
}
u8* ChunkReaderWriter::buf() { return buf_; }
unsigned int ChunkReaderWriter::buflen() const { return sizeof(buf_); }

SocketIO::SocketIO(network::socket_t* socket) : socket_(socket) {}
vecu8 SocketIO::read_chunk(const size_t bytes) {
  size_t to_read = std::min(buflen(), bytes);
  ssize_t res;
#ifdef DEBUG_SOCKETIO
  dprintf("recv sock={},bytes={}", *socket_, bytes);
#endif
  if ((res = network::recv(*socket_, buf_, bytes, 0)) < 0) {
    auto e = network::get_last_network_error();
    if (e == network::ETIMEOUT) {
      throw yrclient::timeout();
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
  size_t to_send = std::min(buflen(), message.size());
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
      throw std::runtime_error(e.what());
    }
  };
  const u32 length = read_obj<u32>(f);
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

// TODO: retries
Connection::Connection(std::string host, std::string port)
    : host_(host), port_(port) {
  // FIXME: set default hints in all overloads
  memset(&hints_, 0, sizeof(hints_));
  hints_.ai_family = AF_UNSPEC;
  hints_.ai_socktype = SOCK_STREAM;
  hints_.ai_protocol = IPPROTO_TCP;
  socket_ = network::BAD_SOCKET;
  {
    network::addrinfo* result;
    network::getaddrinfo(host, port, &hints_, &result);
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

Connection::Connection(std::string port) : port_(port) {
  memset(&hints_, 0, sizeof(hints_));
  hints_.ai_family = AF_INET;
  hints_.ai_socktype = SOCK_STREAM;
  hints_.ai_protocol = IPPROTO_TCP;
  hints_.ai_flags = AI_PASSIVE;
  network::addrinfo* result;
  network::getaddrinfo("", port_, &hints_, &result);
  socket_ = network::socket(result);
  int s = 1;
  network::setsockopt(socket_, network::SOL_SOCKET, network::SO_REUSEADDR,
                      reinterpret_cast<const char*>(&s), sizeof(int));
  if (network::bind(socket_, result->ai_addr, result->ai_addrlen)) {
    throw yrclient::system_error("bind()");
  }

  if (network::listen(socket_, SOMAXCONN)) {
    throw yrclient::system_error("listen()");
  }
}

Connection::Connection(network::socket_t s) : socket_(s) {
  memset(&hints_, 0, sizeof(hints_));
}
Connection::~Connection() {
  try {
    dprintf("closing {}", socket_);
    network::closesocket(socket_);
  } catch (const yrclient::system_error& e) {
    dprintf("closesocket() failed, something's messed up");
  }
  dprintf("sock={}", socket_);
}
// TODO: pass by pointer

int Connection::send_bytes(const vecu8& bytes) {
  connection::SocketIO S(&socket_);
  return write_bytes(bytes, &S);
}
vecu8 Connection::read_bytes() {
  connection::SocketIO S(&socket_);
  return ::read_bytes(&S);
}
network::socket_t Connection::socket() { return socket_; }
