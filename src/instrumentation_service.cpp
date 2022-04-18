#include "instrumentation_service.hpp"
#include "errors.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "protocol/protocol.hpp"
#include <stdexcept>
#include <string>

using namespace yrclient;

command_manager::CommandManager& InstrumentationService::cmd_manager() {
  return cmd_manager_;
}

server::Server& InstrumentationService::server() { return server_; }

yrclient::Response reply_error(std::string message) {
  yrclient::Response R;
  yrclient::ErrorResponse E;
  E.mutable_error_message()->assign(message);
  // E.set_error_message(message);
  R.set_code(RESPONSE_ERROR);

  if (!R.mutable_body()->PackFrom(E)) {
    throw yrclient::general_error("Couldn't pack ACK message");
  }
  return R;
}

yrclient::Response InstrumentationService::process_request(
    connection::Connection* C, vecu8* bytes) {
  yrclient::Response presp;
  // read command from message
  Command cmd;
  if (!cmd.ParseFromArray(bytes->data(), bytes->size())) {
    return reply_error("Message parse error");
  }

  // execute parsed command & write result
  vecu8 result;
  if (cmd.command_type() == yrclient::CLIENT_COMMAND) {
    auto client_cmd = cmd.client_command();
    // schedule command execution
    uint64_t task_id = 0;
    try {
      task_id = cmd_manager().run_command(
          C->socket(), client_cmd.name().c_str(), client_cmd.args());
    } catch (const std::exception& e) {
      return reply_error(e.what());
    }
    // write status back
    Response presp;
    presp.set_code(Response::OK);
    RunCommandAck ack;
    ack.set_id(task_id);
    if (!presp.mutable_body()->PackFrom(ack)) {
      return reply_error("Packing ACK message failed");
    }
    result.resize(presp.ByteSizeLong());
    presp.SerializeToArray(result.data(), result.size());
    return presp;
  } else if (cmd.command_type() == yrclient::POLL) {
    // pop entries from result queue and send them back
    throw yrclient::not_implemented();
  } else {
    return reply_error("Unknown command: " +
                       std::to_string(cmd.command_type()));
  }
  return presp;
}

vecu8 InstrumentationService::on_receive_bytes(connection::Connection* C,
                                               vecu8* bytes) {
  auto response = process_request(C, bytes);
  return to_vecu8(response);
}

void InstrumentationService::on_accept(connection::Connection* C) {
  // Create result queue
  cmd_manager().run_command(cmd_manager().create_queue(C->socket()));
}

void InstrumentationService::on_close(connection::Connection* C) {
  cmd_manager().run_command(cmd_manager().destroy_queue(C->socket()));
}

void InstrumentationService::on_send_bytes(connection::Connection* C,
                                           vecu8* bytes) {}

InstrumentationService::InstrumentationService(const unsigned int max_clients,
                                               const unsigned int port)
    : server_(max_clients, port) {
  server_.callbacks().receive_bytes = [this](auto* c, auto* b) {
    return this->on_receive_bytes(c, b);
  };
  server_.callbacks().send_bytes = [this](auto* c, auto* b) {
    this->on_send_bytes(c, b);
  };
  server_.callbacks().accept = [this](auto* c) { this->on_accept(c); };
  server_.callbacks().close = [this](auto* c) { this->on_close(c); };
}

InstrumentationService::~InstrumentationService() {}
