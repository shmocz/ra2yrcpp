#pragma once
#include "auto_thread.hpp"
#include "network.hpp"
#include "types.h"
#include "utility/sync.hpp"

// TODO(shmocz): namespace pollution
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace ra2yrcpp {

namespace websocket_server {

namespace lib = websocketpp::lib;

///
/// Handles the communication with target TCP socket. Spawns a thread which
/// fetches messages, and sends them through the socket using asio's async
/// read/write functions.
///
class TCPConnection : public std::enable_shared_from_this<TCPConnection> {
 public:
  explicit TCPConnection(lib::asio::ip::tcp::socket s);

  ///
  /// Shutdown (stopping read/write activity on the socket) and close the
  /// underlying socket.
  ///
  void shutdown();

  lib::asio::ip::tcp::socket s_;
  utility::worker_util<std::shared_ptr<vecu8>> f_worker;
};

/// TODO (shmocz): use websocketpp's http handler to allow responding to HTTP
/// requests (CURL for example)
class WebsocketProxy {
 public:
  using server = websocketpp::server<websocketpp::config::asio>;

  struct Options {
    std::string destination;
    unsigned destination_port;
    unsigned websocket_port;
    unsigned max_connections;
  };

  WebsocketProxy(WebsocketProxy::Options o, void* service);
  explicit WebsocketProxy(WebsocketProxy::Options o,
                          lib::asio::io_service* service);
  ~WebsocketProxy();

  void add_connection(network::socket_t src, lib::asio::ip::tcp::socket sock);
  ///
  /// Shutdown all active connections and stop accepting new connections.
  ///
  void shutdown();

  WebsocketProxy::Options opts;
  WebsocketProxy::server s;
  std::map<network::socket_t, std::shared_ptr<TCPConnection>> tcp_conns;
  std::map<network::socket_t, websocketpp::connection_hdl> ws_conns;
  lib::asio::io_service* service_;
  util::AtomicVariable<bool> ready;
};
}  // namespace websocket_server

}  // namespace ra2yrcpp
