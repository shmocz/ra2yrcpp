#pragma once
#include "config.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "logging.hpp"
#include "network.hpp"
#include "process.hpp"
#include "utility/time.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace server {
using network::socket_t;
template <typename T>
using uptr = std::unique_ptr<T>;

class ConnectionCTX {
 public:
  using ctx_fn = std::function<void(ConnectionCTX*)>;
  ConnectionCTX(uptr<connection::Connection> c, ctx_fn main_loop);

  ~ConnectionCTX();
  void join();
  connection::Connection& c();
  process::thread_id_t thread_id;
  std::chrono::system_clock::time_point timestamp() const;

 private:
  uptr<connection::Connection> c_;
  ctx_fn main_loop_;
  std::thread thread_;
  std::chrono::system_clock::time_point timestamp_;
};

struct Callbacks {
  std::function<vecu8(connection::Connection* C, vecu8* bytes)> receive_bytes;
  std::function<void(connection::Connection* C, vecu8* bytes)> send_bytes;
  std::function<void(connection::Connection* C)> accept;
  std::function<void(connection::Connection* C)> close;
};

// TODO: callback for something like "on_ctx_remove"
class Server {
 public:
  Server() = delete;
  Server(unsigned int num_clients = cfg::MAX_CLIENTS,
         unsigned int port = cfg::SERVER_PORT, Callbacks cb = {},
         const unsigned int accept_timeout_ms = cfg::ACCEPT_TIMEOUT_MS);
  ~Server();
  void add_callback();
  /// Main loop spawned for each new connection
  void connection_thread(connection::Connection* C);
  /// Main loop for accepting new connections
  void listener_thread();
  vecu8 on_receive_bytes(connection::Connection* C, vecu8* bytes);
  void on_send_bytes(connection::Connection* C, vecu8* bytes);
  std::string address() const;
  std::string port() const;
  bool is_closing() const;
  Callbacks& callbacks();
  void signal_close();
  size_t num_clients();
  std::vector<uptr<ConnectionCTX>>& connections();

 private:
  const unsigned int max_clients_;
  std::string address_;
  std::string port_;
  Callbacks callbacks_;
  const unsigned int accept_timeout_ms_;
  std::atomic_bool is_closing_;
  std::queue<connection::Connection*> close_queue_;
  connection::Connection listen_connection_;
  std::thread listen_thread_;
  std::vector<uptr<ConnectionCTX>> connections_;
  /// Remove Connections that have been marked as closed from the connections
  /// vector
  void clear_closed();
  std::mutex connections_mut_;
};
}  // namespace server
