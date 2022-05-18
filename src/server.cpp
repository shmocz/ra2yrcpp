#include "server.hpp"
#include "debug_helpers.h"

using namespace server;

Server::Server(unsigned int num_clients, unsigned int port, Callbacks callbacks,
               const unsigned int accept_timeout_ms)
    : max_clients_(num_clients),
      port_(std::to_string(port)),
      callbacks_(callbacks),
      accept_timeout_ms_(accept_timeout_ms),
      listen_connection_(port_),
      is_closing_(false) {
  listen_thread_ = std::thread([this]() { this->listener_thread(); });
}

Server::~Server() {
  // Tell all worker threads to shut down
  is_closing_ = true;
  listen_thread_.join();
}

void Server::connection_thread(connection::Connection* C) {
  if (callbacks().accept) {
    callbacks().accept(C);
  }
  do {
    try {
      auto msg = C->read_bytes();
      auto response = on_receive_bytes(C, &msg);
      on_send_bytes(C, &response);
    } catch (const yrclient::timeout& E) {
      DPRINTF("timeout");
    } catch (const yrclient::system_error& E) {
      DPRINTF("system error: %s\n", E.what());
      // broken connection
      break;
    } catch (const std::runtime_error& E) {
      DPRINTF("runtime error: %s\n", E.what());
      // fatal error
      break;
    }
  } while (!is_closing());
  if (callbacks().close) {
    callbacks().close(C);
  }
}

ConnectionCTX::ConnectionCTX(std::unique_ptr<connection::Connection> c,
                             ctx_fn main_loop)
    : c_(std::move(c)), main_loop_(main_loop), thread_(main_loop_, this) {}

void ConnectionCTX::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

ConnectionCTX::~ConnectionCTX() { join(); }

connection::Connection& ConnectionCTX::c() { return *c_; }

void Server::clear_closed() {
  while (!close_queue_.empty()) {
    auto q = close_queue_.back();
    auto it = std::find_if(
        connections_.begin(), connections_.end(),
        [&](auto& ctx) { return ctx->c().socket() == q->socket(); });
    connections_.erase(it);
    close_queue_.pop();
  }
}

void Server::listener_thread() {
  DPRINTF("listening on port %s\n", port().c_str());
  do {
    socket_t client = 0;
    // Clear up closed connections
    clear_closed();
    auto err = network::accept_connection(this->listen_connection_.socket(),
                                          &client, accept_timeout_ms_);
    if (err == network::ERR_OK) {
      auto conn = std::make_unique<connection::Connection>(client);
      if (connections_.size() >= max_clients_) {
      } else {
        connections_.emplace_back(std::make_unique<ConnectionCTX>(
            std::move(conn), [this](ConnectionCTX* ctx) -> void {
              ctx->thread_id = process::get_current_tid();
              auto c = &ctx->c();
              this->connection_thread(c);
              this->close_queue_.push(c);
            }));
      }
    } else if (err == network::ERR_TIMEOUT) {
    } else {
      // fatal error
      DPRINTF("unknown\n");
    }
  } while (!is_closing());
  DPRINTF("Exit listener\n");
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
bool Server::is_closing() const { return is_closing_; }
server::Callbacks& Server::callbacks() { return callbacks_; }
void Server::signal_close() { is_closing_ = true; }
size_t Server::num_clients() {
  std::unique_lock<std::mutex> lk(connections_mut_);
  return connections_.size();
}
std::vector<uptr<ConnectionCTX>>& Server::connections() { return connections_; }
