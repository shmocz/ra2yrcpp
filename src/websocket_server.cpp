#include "websocket_server.hpp"

#include "asio_utils.hpp"
#include "logging.hpp"
#include "utility/time.hpp"

#include <websocketpp/common/asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/logger/levels.hpp>
#include <websocketpp/server.hpp>

#include <cstddef>

#include <exception>
#include <functional>
#include <memory>
#include <regex>
#include <utility>
#include <vector>

using namespace ra2yrcpp::websocket_server;
using namespace ra2yrcpp::asio_utils;
namespace lib = websocketpp::lib;
using server = websocketpp::server<websocketpp::config::asio>;

class WebsocketServer::server_impl : public server {
 public:
  /// This is so common operation, hence a dedicated method.
  auto get_socket_id(connection_hdl h) {
    return get_con_from_hdl(h)->get_socket().native_handle();
  }
};

WebsocketServer::~WebsocketServer() {}

WSReply::WSReply() {}

struct WSReplyImpl : public WSReply {
  explicit WSReplyImpl(server::message_ptr msg) : msg(msg) {}

  const std::string& get_payload() const override { return msg->get_payload(); }

  int get_opcode() const override { return msg->get_opcode(); }

  server::message_ptr msg;
};

WebsocketServer::WebsocketServer(WebsocketServer::Options o,
                                 ra2yrcpp::asio_utils::IOService* service,
                                 Callbacks cb)
    : opts(o),
      service_(service),
      cb_(cb),
      server_(std::make_unique<server_impl>()) {
  auto& s = *server_.get();
  s.set_message_handler([&](connection_hdl h, auto msg) {
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
      eprintf("got text message, expecting binary");
      return;
    }
    try {
      WSReplyImpl R(msg);
      send_response(h, &R);
    } catch (const std::exception& e) {
      eprintf("error while handling message: {}", e.what());
    }
  });

  // Set TCP_NODELAY flag
  s.set_socket_init_handler([](auto, auto& socket) {
    socket.set_option(asio::ip::tcp::no_delay(true));
  });

  s.set_tcp_pre_init_handler([&](connection_hdl h) {
    auto con = s.get_con_from_hdl(h);
    std::smatch match;
    auto remote = con->get_raw_socket().remote_endpoint().address().to_string();
    if (!std::regex_search(remote, match,
                           std::regex(opts.allowed_hosts_regex))) {
      iprintf("reject connection from {}", remote);
      return;
    }

    iprintf("connection from {}", remote);
  });

  s.set_validate_handler([&](connection_hdl) {
    try {
      if (ws_conns.size() >= opts.max_connections) {
        eprintf("max connections {} exceeded", ws_conns.size());
        return false;
      }
    } catch (const std::exception& e) {
      eprintf("validate_handler: {}", e.what());
      return false;
    }

    return true;
  });

  s.set_close_handler([&](connection_hdl h) {
    try {
      const auto socket_id = s.get_socket_id(h);
      cb_.close(socket_id);
      (void)ws_conns.erase(socket_id);
      iprintf("closed conn {}", socket_id);
    } catch (const std::exception& e) {
      eprintf("close_handler: {}", e.what());
    }
  });

  // TODO(shmocz): thread safety
  s.set_open_handler([&](connection_hdl h) {
    try {
      add_connection(h);
      cb_.accept(s.get_socket_id(h));
    } catch (const std::exception& e) {
      eprintf("open_handler: {}", e.what());
    }
  });

  // TODO(shmocz): thread safety
  s.set_interrupt_handler([&](connection_hdl h) {
    std::size_t count = 0;
    if ((count = ws_conns.erase(s.get_socket_id(h))) < 1) {
      wrprintf("got interrupt, but no connections were removed");
    }
  });

  // HTTP handler for use with CURL etc.
  s.set_http_handler([&](connection_hdl h) {
    auto con = s.get_con_from_hdl(h);
    const auto id = s.get_socket_id(h);
    if (ws_conns.find(id) != ws_conns.end()) {
      eprintf("duplicate connection {}", id);
      return;
    }
    add_connection(h);
    cb_.accept(id);

    ws_conns[id].buffer = con->get_request_body();
    con->defer_http_response();
    ws_conns[id].executor->push(0, [this, con, id](int) {
      auto resp = cb_.receive(id, &ws_conns[id].buffer);
      service_->post(
          [this, con, resp, id]() {
            try {
              con->set_body(resp);
              con->set_status(websocketpp::http::status_code::ok);
              con->send_http_response();
              cb_.close(id);
            } catch (const std::exception& e) {
              eprintf("couldn't send http response: {}", e.what());
            }
            con->interrupt();
          },
          true);
    });
  });

  s.clear_access_channels(websocketpp::log::alevel::frame_payload |
                          websocketpp::log::alevel::frame_header);

#ifdef DEBUG_WEBSOCKETPP
  s.set_access_channels(websocketpp::log::alevel::all);
  s.set_error_channels(websocketpp::log::elevel::all);
#endif
}

// TODO(shmocz): thread safety
void WebsocketServer::shutdown() {
  if (server_->is_listening()) {
    server_->stop_listening();
  }

  // Close WebSocket connections
  service_->post([&]() {
    for (auto& [k, v] : ws_conns) {
      auto cptr = server_->get_con_from_hdl(v.hdl);
      if (cptr->get_state() == websocketpp::session::state::open) {
        server_->close(v.hdl, websocketpp::close::status::normal, "");
      }
    }
    ws_conns.clear();
  });
}

void WebsocketServer::send_response(connection_hdl h, WSReply* msg) {
  auto op = static_cast<websocketpp::frame::opcode::value>(msg->get_opcode());
  auto id = server_->get_socket_id(h);

  ws_conns[id].buffer = msg->get_payload();

  ws_conns[id].executor->push(0, [this, op, id](int) {
    auto& c = ws_conns[id];
    auto resp = cb_.receive(id, &c.buffer);
    service_->post(
        [this, &c, resp, op]() {
          try {
            server_->send(c.hdl, resp, op);
          } catch (...) {
            eprintf("failed to send");
          }
        },
        true);
  });
}

void WebsocketServer::start() {
  dprintf("init asio, port={}", opts.port);
  server_->init_asio(
      static_cast<lib::asio::io_service*>(service_->get_service()));
  server_->set_reuse_addr(true);
  server_->listen(lib::asio::ip::tcp::v4(), opts.port);
  server_->start_accept();
}

void WebsocketServer::add_connection(connection_hdl h) {
  ws_conns[server_->get_socket_id(h)] = {
      h, util::current_time(), "",
      std::make_unique<utility::worker_util<int>>(nullptr, 5)};
}

std::unique_ptr<WebsocketServer> ra2yrcpp::websocket_server::create_server(
    WebsocketServer::Options o, ra2yrcpp::asio_utils::IOService* service,
    WebsocketServer::Callbacks cb) {
  return std::make_unique<WebsocketServer>(o, service, cb);
}
