#include "client_connection.hpp"

using namespace ra2yrcpp::connection;

ClientConnection::ClientConnection(std::string host, std::string port)
    : host(host), port(port), state_(State::NONE) {}

void ClientConnection::send_data(vecu8&& bytes) { send_data(bytes); }

void ClientConnection::stop() {}

util::AtomicVariable<ra2yrcpp::connection::State>& ClientConnection::state() {
  return state_;
}
