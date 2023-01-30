#pragma once
#include "auto_thread.hpp"
#include "connection.hpp"
#include "logging.hpp"
#include "process.hpp"
#include "utility/sync.hpp"
#include "utility/time.hpp"

// FIXME: namespace pollution
#include <asio/error_code.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/server.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ra2yrcpp {

namespace websocket_server {

class WebsocketProxy;

using namespace std::chrono_literals;

using vecu8 = std::vector<u8>;
using wserver = websocketpp::server<websocketpp::config::asio>;

// FIXME: could use start_perpetual instead of IOService
struct IOService {
  websocketpp::lib::asio::io_service s;

  websocketpp::lib::asio::executor_work_guard<decltype(s)::executor_type>
      work_guard;

  IOService() : work_guard(asio::make_work_guard(s)) {}

  void stop() { work_guard.reset(); }
};

struct work_item {
  vecu8 data;
  websocketpp::frame::opcode::value op;

  work_item(vecu8 data, websocketpp::frame::opcode::value op)
      : data(data), op(op) {}
};

class TCPConnection : public std::enable_shared_from_this<TCPConnection> {
 public:
  using wi_type = std::shared_ptr<work_item>;

  explicit TCPConnection(asio::ip::tcp::socket s,
                         websocketpp::lib::asio::io_service* service)
      : s_(std::move(s)), service_(service), f_worker(nullptr) {}

  template <typename FnT>
  void do_read_message_body(FnT f, u32 message_length) {
    auto msg = std::make_shared<std::vector<u8>>(message_length);
    asio::async_read(s_, asio::buffer(*msg),
                     asio::transfer_exactly(msg->size()),
                     [this, f, msg](auto ec, auto length) {
                       (void)length;
                       if (!ec) {
                         f(*msg);
                       } else {
                         eprintf("err={}", ec.message());
                       }
                     });
  }

  // Read message length, then rest of the message
  template <typename FnT>
  void do_read_message(FnT f) {
    auto msg = std::make_shared<std::vector<u8>>(sizeof(u32));
    asio::async_read(
        s_, asio::buffer(*msg), asio::transfer_exactly(msg->size()),
        [this, f, msg](auto ec, auto length) {
          (void)length;
          if (!ec) {
            do_read_message_body(f, serialize::read_obj_le<u32>(msg->data()));
          } else {
            eprintf("err={}", ec.message());
          }
        });
  }

  template <typename FnT>
  void do_write(const std::vector<u8>& data, FnT f) {
    shared_from_this();
    u32 sz = data.size();
    auto msg = std::make_shared<std::vector<u8>>(sizeof(sz));
    // put length
    std::vector<u8> ssz(sizeof(u32), 0);
    std::copy(reinterpret_cast<char*>(&sz),
              (reinterpret_cast<char*>(&sz)) + sizeof(sz), msg->begin());
    // put rest of the msg
    msg->insert(msg->end(), data.begin(), data.end());

    asio::async_write(s_, asio::buffer(*msg),
                      asio::transfer_exactly(msg->size()),
                      [this, f, msg](auto ec, auto length) {
                        if (length != msg->size()) {
                          std::abort();
                        }
                        if (!ec) {
                          do_read_message(f);
                        } else {
                          eprintf("err={}", ec.message());
                        }
                      });
  }

  asio::ip::tcp::socket s_;
  asio::ip::tcp::endpoint e_;
  websocketpp::lib::asio::io_service* service_;
  utility::worker_util<wi_type> f_worker;
};

class WebsocketProxy {
 public:
  using server = websocketpp::server<websocketpp::config::asio>;

  struct Options {
    std::string destination;
    unsigned destination_port;
    unsigned websocket_port;
    unsigned max_connections;
  };

  explicit WebsocketProxy(WebsocketProxy::Options o,
                          websocketpp::lib::asio::io_service* service);

  void add_connection(network::socket_t src, asio::ip::tcp::socket sock);

  WebsocketProxy::Options opts;
  WebsocketProxy::server s;
  std::map<network::socket_t, std::shared_ptr<TCPConnection>> tcp_conns;
  websocketpp::lib::asio::io_service* service_;
  util::Event<> ready;
};
}  // namespace websocket_server

}  // namespace ra2yrcpp
