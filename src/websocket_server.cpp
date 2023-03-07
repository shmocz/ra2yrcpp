#include "websocket_server.hpp"

using namespace ra2yrcpp::websocket_server;

IOService::IOService()
    : work_guard(asio::make_work_guard(s)),
      main_thread([this]() { s.run(); }) {}

IOService::~IOService() {
  work_guard.reset();
  main_thread.join();
}

void WebsocketProxy::add_connection(network::socket_t src,
                                    asio::ip::tcp::socket sock) {
  if (tcp_conns.find(src) == tcp_conns.end()) {
    tcp_conns[src] = std::make_shared<TCPConnection>(std::move(sock), service_);

  } else {
    eprintf("duplicate socket: {}", src);
  }
}

WebsocketProxy::WebsocketProxy(WebsocketProxy::Options o,
                               websocketpp::lib::asio::io_service* service)
    : opts(o), service_(service), ready(false) {
  // connect to destination server
  s.set_message_handler([this](auto h, auto msg) {
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
      eprintf("got text message, expecting binary");
    } else {
      // get connection associated with this client
      asio::error_code ec;
      auto cptr = s.get_con_from_hdl(h, ec);
      if (ec) {
        eprintf("error: {}", ec.message());
        return;
      }
      auto p = msg->get_payload();
      auto op = msg->get_opcode();

      auto* conn = tcp_conns[cptr->get_socket().native_handle()].get();

      conn->f_worker.push(
          std::make_shared<work_item>(std::vector<u8>(p.begin(), p.end()), op),
          [this, conn, h](auto& w) {
            conn->do_write(w->data, [this, h, w](auto r) {
              s.send(h, reinterpret_cast<void*>(r.data()), r.size(), w->op);
            });
          });
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
      asio::error_code ec;
      auto cptr = s.get_con_from_hdl(h, ec);
      if (ec) {
        eprintf("error: {}", ec.message());
        return false;
      }

      // Connect to IService
      asio::ip::tcp::socket sock(*service_);
      sock.connect(
          asio::ip::tcp::endpoint{
              asio::ip::address_v4::from_string(opts.destination),
              static_cast<u16>(opts.destination_port)},
          ec);

      if (ec) {
        eprintf("TCP connection to service failed: {}", ec.message());
        return false;
      }

      add_connection(cptr->get_socket().native_handle(), std::move(sock));
    } catch (const std::exception& e) {
      eprintf("fail={}", e.what());
      return false;
    }

    return true;
  });

  // Erase proxied TCP connection
  s.set_close_handler([this](auto h) {
    try {
      auto cptr = s.get_con_from_hdl(h);

      (void)tcp_conns.erase(cptr->get_socket().native_handle());
    } catch (const std::exception& e) {
      eprintf("fail={}", e.what());
    }
  });

  s.set_fail_handler([this](auto h) {
    asio::error_code ec;
    auto cptr = s.get_con_from_hdl(h, ec);
    eprintf("fail handle={}", cptr->get_socket().native_handle());
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
  s.listen(websocketpp::lib::asio::ip::tcp::v4(), o.websocket_port);
  s.start_accept();
  service_->post([this]() { ready.store(true); });
}

WebsocketProxy::~WebsocketProxy() { s.stop_listening(); }
