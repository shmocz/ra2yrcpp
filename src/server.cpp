#include "config.hpp"
#include "connection.hpp"
#include "debug_helpers.h"
#include "errors.hpp"
#include "network.hpp"
#include "server.hpp"
#include <string>
#include <stdexcept>

using namespace server;

Server::Server(unsigned int num_clients, unsigned int port, Callbacks callbacks)
    : max_clients_(num_clients),
      port_(std::to_string(port)),
      callbacks_(callbacks),
      listen_connection_(port_),
      is_closing_(false) {
  listen_thread_ = std::thread([this]() { this->listener_thread(); });
}

Server::~Server() {
  // Tell all worker threads to shut down
  is_closing_ = true;

  // Wait
  for (auto& C : connections_) {
    C.join();
  }

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

ConnectionCTX::ConnectionCTX(
    socket_t s, std::function<void(connection::Connection*)> main_loop)
    : c_(s), main_loop_(main_loop), thread_(main_loop_, &c_) {}

void ConnectionCTX::join() { thread_.join(); }

void Server::listener_thread() {
  do {
    socket_t client = 0;
    // TODO: dont hardcode timeout
    auto err = network::accept_connection(this->listen_connection_.socket(),
                                          &client, cfg::ACCEPT_TIMEOUT_MS);
    if (err == network::ERR_OK) {
      connections_.emplace_back(client,
                                [this](connection::Connection* c) -> void {
                                  this->connection_thread(c);
                                });
    } else if (err == network::ERR_TIMEOUT) {
      DPRINTF("timeout\n");
    } else {
      // fatal error
      DPRINTF("unknown\n");
    }
  } while (!is_closing());
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
Server::Callbacks& Server::callbacks() { return callbacks_; }
void Server::signal_close() { is_closing_ = true; }
