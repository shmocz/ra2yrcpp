#include "websocket_connection.hpp"

#include "asio_utils.hpp"
#include "client_connection.hpp"
#include "config.hpp"
#include "logging.hpp"
#include "utility/sync.hpp"

#include <fmt/core.h>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <vector>

using namespace ra2yrcpp::connection;
using namespace std::chrono_literals;

using ws_error = websocketpp::lib::error_code;

class ClientWebsocketConnection::client_impl
    : public websocketpp::client<websocketpp::config::asio_client> {};

void ClientWebsocketConnection::stop() {
  in_q_.push(std::make_shared<vecu8>());
}

void ClientWebsocketConnection::connect() {
  state_.store(State::CONNECTING);
  ws_error ec;
  auto* c_ = client_.get();

#ifdef DEBUG_WEBSOCKETPP
  c_->set_access_channels(websocketpp::log::alevel::all);
  c_->set_error_channels(websocketpp::log::elevel::all);
#endif

  c_->init_asio(reinterpret_cast<asio::io_service*>(io_service_->get_service()),
                ec);

  if (ec) {
    throw std::runtime_error("init_asio() failed: " + ec.message());
  }

  c_->clear_access_channels(websocketpp::log::alevel::frame_payload |
                            websocketpp::log::alevel::frame_header);
  c_->set_fail_handler([this, c_](auto h) {
    ws_error ec_;
    wrprintf("fail handler={}",
             c_->get_con_from_hdl(h, ec_)->get_socket().native_handle());
    stop();
    state_.store(State::CLOSED);
  });

  c_->set_message_handler([this](auto, auto msg) {
    try {
      auto p = msg->get_payload();
      in_q_.push(std::make_shared<vecu8>(p.begin(), p.end()));
    } catch (websocketpp::exception const& e) {
      eprintf("message_handler: {}", e.what());
    }
  });
  c_->set_open_handler([this](auto h) {
    connection_handle_ = h;
    state_.store(State::OPEN);
  });

  c_->set_close_handler([this](auto) { state_.store(State::CLOSED); });

  auto con = c_->get_connection(std::string("ws://" + host + ":" + port), ec);

  if (ec) {
    throw std::runtime_error(ec.message());
  }

  c_->connect(con);
  state_.wait_pred([](auto state) {
    return state == State::OPEN || state == State::CLOSED;
  });
  if (state() != State::OPEN) {
    throw std::runtime_error("failed to open connection, state={}" +
                             std::to_string(static_cast<int>(state().get())));
  }
}

void ClientWebsocketConnection::send_data(const vecu8& bytes) {
  ws_error ec;
  io_service_->post([this, &bytes, &ec]() {
    auto* c_ = client_.get();
    c_->send(connection_handle_, bytes.data(), bytes.size(),
             websocketpp::frame::opcode::binary, ec);
  });
  if (ec) {
    throw std::runtime_error(
        fmt::format("failed to send data: {}", ec.message()));
  }
}

vecu8 ClientWebsocketConnection::read_data() {
  try {
    auto res = in_q_.pop(
        1, std::chrono::duration_cast<duration_t>(cfg::WEBSOCKET_READ_TIMEOUT));
    return *res.at(0);
  } catch (const std::exception& e) {
    throw std::runtime_error(
        std::string("failed to read data (likely connection closed): ") +
        e.what());
  }
}

ClientWebsocketConnection::ClientWebsocketConnection(
    std::string host, std::string port,
    ra2yrcpp::asio_utils::IOService* io_service)
    : ClientConnection(host, port),
      client_(std::make_unique<client_impl>()),
      io_service_(io_service) {}

ClientWebsocketConnection::~ClientWebsocketConnection() {
  try {
    client_->close(connection_handle_, websocketpp::close::status::going_away,
                   "");
    iprintf("websocket connection closed");
  } catch (const std::exception& e) {
    wrprintf("failed to close gracefully: {}", e.what());
  }
  state_.wait(State::CLOSED);
}
