#pragma once
#include "config.hpp"
#include "connection.hpp"
#include "network.hpp"
#include <atomic>
#include <functional>
#include <thread>

namespace server {
using network::socket_t;

class ConnectionCTX {
 public:
  ConnectionCTX(socket_t s,
                std::function<void(connection::Connection*)> main_loop);
  void join();

 private:
  connection::Connection c_;
  std::function<void(connection::Connection*)> main_loop_;
  std::thread thread_;
};

class Server {
  struct Callbacks {
    std::function<vecu8(connection::Connection* C, vecu8* bytes)> receive_bytes;
    std::function<void(connection::Connection* C, vecu8* bytes)> send_bytes;
  };

 public:
  Server() = delete;
  Server(unsigned int num_clients = cfg::MAX_CLIENTS,
         unsigned int port = cfg::SERVER_PORT, Callbacks cb = {});
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

 private:
  const unsigned int max_clients_;
  std::string address_;
  std::string port_;
  std::thread listen_thread_;
  Callbacks callbacks_;
  connection::Connection listen_connection_;
  std::atomic_bool is_closing_;
  std::vector<ConnectionCTX> connections_;
};
}  // namespace server
