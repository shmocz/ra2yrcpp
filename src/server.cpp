#include "server.hpp"

using namespace server;

Server::Server(unsigned int num_clients, unsigned int port, Callbacks callbacks,
               const unsigned int accept_timeout_ms)
    : max_clients_(num_clients),
      port_(std::to_string(port)),
      callbacks_(callbacks),
      accept_timeout_ms_(accept_timeout_ms),
      state_(STATE::NONE),
      listen_connection_(port_),
      listen_thread_([this]() {
        try {
          this->listener_thread();
        } catch (const std::exception& e) {
          eprintf("listener died {}", e.what());
        }
      }) {}

Server::~Server() {
  // Tell all worker threads to shut down
  state_.store(STATE::CLOSING);
  listen_thread_.join();
  state_.store(STATE::CLOSED);
}

void Server::connection_thread(connection::Connection* C) {
  if (callbacks().accept) {
    callbacks().accept(C);
  }
  dprintf("accepted, sock={}", C->socket());
  network::set_io_timeout(C->socket(), cfg::SOCKET_SR_TIMEOUT);
  do {
    try {
      auto msg = C->read_bytes();
      if (msg.empty()) {
        dprintf("connection closed");
        break;
      }
      auto response = on_receive_bytes(C, &msg);
      on_send_bytes(C, &response);
    } catch (const yrclient::timeout& E) {
    } catch (const std::exception& e) {
      eprintf("fatal error: {}", e.what());
      break;
    }
  } while (!is_closing());
  dprintf("exiting, sock={}", C->socket());
  if (callbacks().close) {
    callbacks().close(C);
  }
}

ConnectionCTX::ConnectionCTX(std::unique_ptr<connection::Connection> c,
                             ctx_fn main_loop)
    : c_(std::move(c)),
      main_loop_(main_loop),
      thread_(main_loop_, this),
      timestamp_(util::current_time()) {}

void ConnectionCTX::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

ConnectionCTX::~ConnectionCTX() { join(); }

connection::Connection& ConnectionCTX::c() { return *c_; }

std::chrono::system_clock::time_point ConnectionCTX::timestamp() const {
  return timestamp_;
}

void Server::clear_closed() {
  std::lock_guard<std::mutex> lk(connections_mut);
  while (!close_queue_.empty()) {
    auto q = close_queue_.front();
    auto it = std::find_if(
        connections_.begin(), connections_.end(),
        [&](auto& ctx) { return ctx->c().socket() == q->socket(); });
    connections_.erase(it);
    close_queue_.pop_front();
  }
}

void Server::listener_thread() {
  dprintf("listening on port {}", port().c_str());
  state_.store(STATE::ACTIVE);
  do {
    socket_t client = 0;
    // Clear up closed connections
    clear_closed();
    auto err = network::accept_connection(this->listen_connection_.socket(),
                                          &client, accept_timeout_ms_);
    if (err == network::ERR_OK) {
      auto conn = std::make_unique<connection::Connection>(client);
      if (connections_.size() >= max_clients_) {
        eprintf("rejecting connection, max size {} exceeded",
                connections_.size());
      } else {
        dprintf("new conn, sock={}", conn.get()->socket());
        std::lock_guard<std::mutex> lk(connections_mut);
        connections_.emplace_back(std::make_unique<ConnectionCTX>(
            std::move(conn), [this](ConnectionCTX* ctx) -> void {
              ctx->thread_id = process::get_current_tid();
              auto c = &ctx->c();
              try {
                this->connection_thread(c);
              } catch (const std::exception& e) {
                eprintf("connection died {}", e.what());
              }
              dprintf("closing");
              std::lock_guard<std::mutex> lk_(this->connections_mut);
              // Signal listener thread to destroy the ConnectionCTX object. We
              // cannot just remove ourselves, because we would join to the same
              // thread we are in.
              this->close_queue_.push_front(c);
            }));
      }
    } else if (err == network::ERR_TIMEOUT) {
    } else {
      eprintf("unknown error ({})", network::get_last_network_error());
      break;
    }
  } while (!is_closing());
  dprintf("Exit listener");
}

vecu8 Server::on_receive_bytes(connection::Connection* C, vecu8* bytes) {
  return callbacks_.receive_bytes(C, bytes);
}

void Server::on_send_bytes(connection::Connection* C, vecu8* bytes) {
  callbacks_.send_bytes(C, bytes);
  C->send_bytes(*bytes);
}

std::string Server::address() const { return address_; }

std::string Server::port() const { return port_; }

bool Server::is_closing() { return state_.get() == STATE::CLOSING; }

server::Callbacks& Server::callbacks() { return callbacks_; }

std::vector<uptr<ConnectionCTX>>& Server::connections() { return connections_; }

std::deque<connection::Connection*>& Server::close_queue() {
  return close_queue_;
}

util::AtomicVariable<Server::STATE>& Server::state() { return state_; }
