#include "websocket_server.hpp"

#include "logging.hpp"
#include "utility/serialize.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

using namespace ra2yrcpp::websocket_server;
using error_code = lib::error_code;

template <typename FnT>
static void do_read_message_body(TCPConnection* C, FnT f, u32 message_length) {
  auto msg = std::make_shared<vecu8>(message_length);
  // bind the completion handler to connection object
  auto self(C->shared_from_this());
  lib::asio::async_read(C->s_, lib::asio::buffer(*msg),
                        lib::asio::transfer_exactly(msg->size()),
                        [C, f, msg, self](auto ec, std::size_t) {
                          if (!ec) {
                            f(*msg);
                          } else {
                            eprintf("err={}", ec.message());
                          }
                        });
}

template <typename FnT>
static void do_read_message(TCPConnection* C, FnT f) {
  auto msg = std::make_shared<vecu8>(sizeof(u32));

  auto self(C->shared_from_this());
  lib::asio::async_read(
      C->s_, lib::asio::buffer(*msg), lib::asio::transfer_exactly(msg->size()),
      [C, f, msg, self](auto ec, std::size_t) {
        if (!ec) {
          do_read_message_body(C, f, serialize::read_obj_le<u32>(msg->data()));
        } else {
          eprintf("err={}", ec.message());
        }
      });
}

// TODO: create a common method for length-prefixed message handling, and
// refactor the stuff in connection.hpp to use it properly.
template <typename FnT>
static void do_write(TCPConnection* C, const vecu8& data, FnT f) {
  u32 sz = data.size();
  auto msg = std::make_shared<vecu8>(sizeof(sz));
  // put length
  vecu8 ssz(sizeof(u32), 0);
  std::copy(reinterpret_cast<char*>(&sz),
            (reinterpret_cast<char*>(&sz)) + sizeof(sz), msg->begin());
  // put rest of the msg
  msg->insert(msg->end(), data.begin(), data.end());

  auto self(C->shared_from_this());
  lib::asio::async_write(C->s_, lib::asio::buffer(*msg),
                         lib::asio::transfer_exactly(msg->size()),
                         [C, f, msg, self](auto ec, std::size_t) {
                           if (!ec) {
                             do_read_message(C, f);
                           } else {
                             eprintf("err={}", ec.message());
                           }
                         });
}

TCPConnection::TCPConnection(lib::asio::ip::tcp::socket s)
    : s_(std::move(s)), f_worker(nullptr) {}

void TCPConnection::shutdown() {
  s_.shutdown(asio::socket_base::shutdown_both);
  s_.close();
}

void WebsocketProxy::add_connection(network::socket_t src,
                                    lib::asio::ip::tcp::socket sock) {
  if (tcp_conns.find(src) == tcp_conns.end()) {
    tcp_conns[src] = std::make_shared<TCPConnection>(std::move(sock));
  } else {
    eprintf("duplicate socket: {}", src);
  }
}

WebsocketProxy::WebsocketProxy(WebsocketProxy::Options o, void* service)
    : WebsocketProxy(o, reinterpret_cast<lib::asio::io_service*>(service)) {}

WebsocketProxy::WebsocketProxy(WebsocketProxy::Options o,
                               lib::asio::io_service* service)
    : opts(o), service_(service), ready(false) {
  // connect to destination server
  s.set_message_handler([this](auto h, auto msg) {
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
      eprintf("got text message, expecting binary");
      return;
    }
    // get connection associated with this client
    error_code ec;
    try {
      auto cptr = s.get_con_from_hdl(h);
      auto p = msg->get_payload();
      auto op = msg->get_opcode();

      auto* conn = tcp_conns[cptr->get_socket().native_handle()].get();

      conn->f_worker.push(std::make_shared<vecu8>(p.begin(), p.end()),
                          [this, conn, h, op](auto& data) {
                            do_write(conn, *data.get(), [this, h, op](auto r) {
                              s.send(h, reinterpret_cast<void*>(r.data()),
                                     r.size(), op);
                            });
                          });
    } catch (const std::exception& e) {
      eprintf("error while handling message: {}", e.what());
    }
  });

  // Set TCP_NODELAY flag
  s.set_socket_init_handler([](auto, auto& socket) {
    socket.set_option(asio::ip::tcp::no_delay(true));
  });

  // Establish TCP connection to InstrumentationService
  s.set_validate_handler([this](auto h) {
    try {
      if (tcp_conns.size() >= opts.max_connections) {
        eprintf("max connections {} exceeded", tcp_conns.size());
        return false;
      }
      error_code ec;
      auto cptr = s.get_con_from_hdl(h);

      // Connect to IService
      lib::asio::ip::tcp::socket sock(*service_);
      sock.connect(
          lib::asio::ip::tcp::endpoint{
              lib::asio::ip::address_v4::from_string(opts.destination),
              static_cast<u16>(opts.destination_port)},
          ec);

      if (ec) {
        eprintf("TCP connection to service failed: {}", ec.message());
        return false;
      }

      add_connection(cptr->get_socket().native_handle(), std::move(sock));
    } catch (const std::exception& e) {
      eprintf("validate_handler: {}", e.what());
      return false;
    }

    return true;
  });

  // Erase proxied TCP connection
  s.set_close_handler([this](auto h) {
    try {
      auto cptr = s.get_con_from_hdl(h);
      auto id = cptr->get_socket().native_handle();

      tcp_conns[id]->shutdown();
      (void)tcp_conns.erase(id);
      (void)ws_conns.erase(id);
    } catch (const std::exception& e) {
      eprintf("close_handler: {}", e.what());
    }
  });

  s.set_open_handler([this](auto h) {
    try {
      auto cptr = s.get_con_from_hdl(h);
      ws_conns[cptr->get_socket().native_handle()] = h;
    } catch (const std::exception& e) {
      eprintf("open_handler: {}", e.what());
    }
  });

  s.clear_access_channels(websocketpp::log::alevel::frame_payload |
                          websocketpp::log::alevel::frame_header);

#ifdef DEBUG_WEBSOCKETPP
  s.set_access_channels(websocketpp::log::alevel::all);
  s.set_error_channels(websocketpp::log::elevel::all);
#endif

  dprintf("init asio, ws_port={}, dest_port={}", o.websocket_port,
          o.destination_port);
  s.init_asio(service_);
  s.set_reuse_addr(true);
  s.listen(lib::asio::ip::tcp::v4(), o.websocket_port);
  s.start_accept();
  service_->post([this]() { ready.store(true); });
}

WebsocketProxy::~WebsocketProxy() { shutdown(); }

void WebsocketProxy::shutdown() {
  if (s.is_listening()) {
    s.stop_listening();
  }

  // Close TCP connections
  for (auto& [k, v] : tcp_conns) {
    v->shutdown();
  }

  // Close WebSocket connections
  for (auto& [k, v] : ws_conns) {
    s.close(v, websocketpp::close::status::going_away, "");
  }
}
