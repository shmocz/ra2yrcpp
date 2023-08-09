#pragma once
#include "auto_thread.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

namespace ra2yrcpp {

namespace websocket_server {

using connection_hdl = std::weak_ptr<void>;

/// This abstracts away websocketpp's message_ptr
struct WSReply {
  WSReply();
  virtual ~WSReply() = default;
  virtual const std::string& get_payload() const = 0;
  virtual int get_opcode() const = 0;
};

struct SocketEntry {
  connection_hdl hdl;
  std::chrono::system_clock::time_point timestamp;
  std::string buffer;
  std::unique_ptr<utility::worker_util<int>> executor;
};

class WebsocketServer {
 public:
  using socket_t = int;

  struct Options {
    std::string host;
    unsigned port;
    unsigned max_connections;
  };

  struct Callbacks {
    std::function<std::string(socket_t, std::string*)> receive;
    std::function<void(socket_t)> accept;
    std::function<void(socket_t)> close;
  };

  WebsocketServer() = delete;
  WebsocketServer(WebsocketServer::Options o,
                  ra2yrcpp::asio_utils::IOService* service, Callbacks cb);
  ~WebsocketServer();

  ///
  void start();
  /// Shutdown all active connections and stop accepting new connections.
  void shutdown();
  /// Send a reply for a previously received message. Must be used within
  /// io_service's thread.
  void send_response(connection_hdl h, WSReply* msg);

  /// Add a recently accepted connection to internal connection list.
  void add_connection(connection_hdl h);

  WebsocketServer::Options opts;
  std::map<unsigned int, SocketEntry> ws_conns;
  ra2yrcpp::asio_utils::IOService* service_;
  Callbacks cb_;
  class server_impl;
  std::unique_ptr<server_impl> server_;
};

std::unique_ptr<WebsocketServer> create_server(
    WebsocketServer::Options o, ra2yrcpp::asio_utils::IOService* service,
    WebsocketServer::Callbacks cb);
}  // namespace websocket_server

}  // namespace ra2yrcpp
