#include "websocket_connection.hpp"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/frame.hpp>

using namespace connection;
using namespace std::chrono_literals;

using client_t = websocketpp::client<websocketpp::config::asio_client>;

void ClientWebsocketConnection::stop() {
  in_q_.push(std::make_shared<vecu8>());
}

void ClientWebsocketConnection::connect() {
  state_.store(State::CONNECTING);
  websocketpp::lib::error_code ec;
  auto c_ = reinterpret_cast<client_t*>(client_.get());

#ifdef DEBUG_WEBSOCKETPP
  c_->set_access_channels(websocketpp::log::alevel::all);
  c_->set_error_channels(websocketpp::log::elevel::all);
#endif

  c_->init_asio(
      reinterpret_cast<websocketpp::lib::asio::io_service*>(io_service_), ec);

  if (ec) {
    throw std::runtime_error("init_asio() failed: " + ec.message());
  }

  c_->clear_access_channels(websocketpp::log::alevel::frame_payload |
                            websocketpp::log::alevel::frame_header);
  c_->set_fail_handler([this, c_](auto h) {
    websocketpp::lib::error_code ec_;
    eprintf("fail handle={}",
            c_->get_con_from_hdl(h, ec_)->get_socket().native_handle());
    stop();
    state_.store(State::CLOSED);
  });

  c_->set_message_handler([this, c_](auto hdl, auto msg) {
    try {
      auto p = msg->get_payload();
      in_q_.push(std::make_shared<vecu8>(p.begin(), p.end()));
    } catch (websocketpp::exception const& e) {
      eprintf("FIXME error!");
    }
  });
  c_->set_open_handler([this, c_](auto h) {
    connection_handle_ = h;
    state_.store(State::OPEN);
  });

  c_->set_close_handler([this, c_](auto h) { state_.store(State::CLOSED); });

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
                             std::to_string(static_cast<int>(state())));
  }
}

bool ClientWebsocketConnection::send_data(const std::vector<u8>& bytes) {
  // TODO: could use websocket's interrupt_handler
  // TODO: try use send method with a callback to get actual bytes sent
  util::AtomicVariable<bool> done(false);
  reinterpret_cast<websocketpp::lib::asio::io_service*>(io_service_)
      ->post([this, &bytes, &done]() {
        auto c_ = reinterpret_cast<client_t*>(client_.get());
        websocketpp::lib::error_code ec;
        c_->send(connection_handle_, bytes.data(), bytes.size(),
                 websocketpp::frame::opcode::binary, ec);
        if (ec) {
          eprintf("failed to send data: {}", ec.message());
        }
        done.store(true);
      });
  done.wait(true);
  return true;
}

vecu8 ClientWebsocketConnection::read_data() {
  try {
    auto res = in_q_.pop(1, 5000ms);
    return *res.at(0);
  } catch (const std::exception& e) {
    throw std::runtime_error(
        std::string("failed to read data (likely connection closed): ") +
        e.what());
  }
}

ClientWebsocketConnection::ClientWebsocketConnection(std::string host,
                                                     std::string port,
                                                     void* io_service)
    : ClientConnection(host, port),
      client_(utility::make_uptr<client_t>()),
      io_service_(io_service) {}

ClientWebsocketConnection::~ClientWebsocketConnection() {
  reinterpret_cast<client_t*>(client_.get())
      ->close(connection_handle_, websocketpp::close::status::normal, "");
  state_.wait(State::CLOSED);
}
