#include "multi_client.hpp"

#include "protocol/protocol.hpp"

#include "errors.hpp"
#include "logging.hpp"
#include "ra2yrproto/commands_builtin.pb.h"
#include "websocket_connection.hpp"

#include <fmt/core.h>
#include <google/protobuf/repeated_ptr_field.h>

#include <array>
#include <exception>
#include <stdexcept>

namespace google {
namespace protobuf {
class Message;
}
}  // namespace google

using namespace multi_client;
using connection::State;

static connection::ClientConnection* get_connection(const std::string host,
                                                    const std::string port,
                                                    CONNECTION_TYPE t,
                                                    void* io_service) {
  if (t == CONNECTION_TYPE::TCP) {
    return new connection::ClientTCPConnection(host, port);
  } else if (t == CONNECTION_TYPE::WEBSOCKET) {
    return new connection::ClientWebsocketConnection(host, port, io_service);
  } else {
    throw std::runtime_error("invalid connection type");
  }
}

AutoPollClient::AutoPollClient(const std::string host, const std::string port,
                               const duration_t poll_timeout,
                               const duration_t command_timeout,
                               CONNECTION_TYPE ctype, void* io_service)
    : host_(host),
      port_(port),
      poll_timeout_(poll_timeout),
      command_timeout_(command_timeout),
      ctype_(ctype),
      io_service_(io_service),
      state_(State::NONE) {
  static constexpr std::array<ClientType, 2> t = {ClientType::COMMAND,
                                                  ClientType::POLL};

  for (auto i : t) {
    auto* c = get_connection(host, port, ctype_, io_service_);
    is_clients_[i] = std::make_unique<InstrumentationClient>(
        std::shared_ptr<connection::ClientConnection>(c));

    is_clients_[i]->connection()->connect();

    // send initial "handshake" message
    // this will also ensure we fail early in case of connection errors
    ra2yrproto::commands::GetSystemState cmd_gs;

    auto r_resp =
        is_clients_[i]->send_command(cmd_gs, ra2yrproto::CLIENT_COMMAND);
    auto ack = yrclient::from_any<ra2yrproto::RunCommandAck>(r_resp.body());
    queue_ids_[i] = ack.queue_id();
    is_clients_[i]->poll_blocking(poll_timeout_, queue_ids_[i]);
  }
  poll_thread_ = std::thread([this]() { poll_thread(); });
}

AutoPollClient::AutoPollClient(AutoPollClient::Options o)
    : AutoPollClient(o.host, o.port, o.poll_timeout, o.command_timeout, o.ctype,
                     o.io_service) {}

// FIXME: this won't be called if constructor throws
AutoPollClient::~AutoPollClient() {
  // signal poll thread to exit
  state_.store(State::CLOSING);
  poll_thread_.join();

  // stop both connections
  get_client(ClientType::POLL)->connection()->stop();
  get_client(ClientType::COMMAND)->connection()->stop();
}

// TODO: as the command is synchronous, cmd's memory remains valid throughout
// the entire call, eliminating the need for extra copies.
ra2yrproto::Response AutoPollClient::send_command(
    const google::protobuf::Message& cmd) {
  // Send command
  auto resp = get_client(ClientType::COMMAND)
                  ->send_command(cmd, ra2yrproto::CommandType::CLIENT_COMMAND);
  auto ack = yrclient::from_any<ra2yrproto::RunCommandAck>(resp.body());
  // Wait until item found from polled messages
  try {
    // TODO(shmocz): signal if poll_thread dies
    auto r = yrclient::make_response(results().get(ack.id(), command_timeout_),
                                     yrclient::RESPONSE_OK);
    results().erase(ack.id());
    return r;
  } catch (const std::runtime_error& e) {
    throw yrclient::general_error(fmt::format(
        "timeout after {}ms, key={}", command_timeout_.count(), ack.id()));
  }
}

void AutoPollClient::poll_thread() {
  // wait connection to be established
  get_client(ClientType::POLL)->connection()->state().wait_pred([](auto v) {
    return v == State::OPEN;
  });
  state_.store(State::OPEN);

  while (state_.get() == State::OPEN) {
    try {
      // TODO(shmocz): return immediately if signaled to stop
      auto R =
          get_client(ClientType::POLL)
              ->poll_blocking(poll_timeout_, get_queue_id(ClientType::COMMAND));
      for (auto& r : R.result().results()) {
        results_.put(r.command_id(), r);
      }
    } catch (const yrclient::timeout& e) {
    } catch (const yrclient::system_error& e) {
      eprintf("internal error, likely cmd connection exit: {}", e.what());
    } catch (const std::exception& e) {
      eprintf("fatal error: {}", e.what());
    }
  }
}

ResultMap& AutoPollClient::results() { return results_; }

InstrumentationClient* AutoPollClient::get_client(const ClientType type) {
  return is_clients_[type].get();
}

u64 AutoPollClient::get_queue_id(ClientType t) const {
  return queue_ids_.at(t);
}
