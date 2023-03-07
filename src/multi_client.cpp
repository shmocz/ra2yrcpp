#include "multi_client.hpp"

using namespace multi_client;

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
                               const std::chrono::milliseconds poll_timeout,
                               const std::chrono::milliseconds command_timeout,
                               CONNECTION_TYPE ctype, void* io_service)
    : host_(host),
      port_(port),
      poll_timeout_(poll_timeout),
      command_timeout_(command_timeout),
      ctype_(ctype),
      io_service_(io_service),
      active_(false) {
  static constexpr std::array<ClientType, 2> t = {ClientType::COMMAND,
                                                  ClientType::POLL};

  for (auto i : t) {
    auto* c = get_connection(host, port, ctype_, io_service_);
    // FIXME
    // network::set_io_timeout(c->socket(), 10000);
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
  // TODO: ensure that poll client is properly initialized
  active_ = true;
  poll_thread_ = std::thread([this]() { poll_thread(); });
}

AutoPollClient::AutoPollClient(AutoPollClient::Options o)
    : AutoPollClient(o.host, o.port, o.poll_timeout, o.command_timeout, o.ctype,
                     o.io_service) {}

AutoPollClient::~AutoPollClient() {
  active_ = false;
  // FIXME: this wont be called if constructor throws
  get_client(ClientType::POLL)->connection()->stop();
  get_client(ClientType::COMMAND)->connection()->stop();

  // FIXME: blocks if max conns achieved
  poll_thread_.join();
}

// TODO: since this is essentially synchronous command, the memory of cmd is
// valid throughout the entire call, so we may not need to make unnecessary
// copies of it
ra2yrproto::Response AutoPollClient::send_command(
    const google::protobuf::Message& cmd) {
  // Send command
  auto resp = get_client(ClientType::COMMAND)
                  ->send_command(cmd, ra2yrproto::CommandType::CLIENT_COMMAND);
  auto ack = yrclient::from_any<ra2yrproto::RunCommandAck>(resp.body());
  // Wait until item found from polled messages
  try {
    // FIXME: signal if poll_thread dies
    auto r = yrclient::make_response(results().get(ack.id(), command_timeout_),
                                     yrclient::RESPONSE_OK);
    results().erase(ack.id());
    return r;
  } catch (const std::runtime_error& e) {
    eprintf("timeout after {}ms, key={}", command_timeout_.count(), ack.id());
    throw yrclient::general_error(fmt::format(
        "timeout after {}ms, key={}", command_timeout_.count(), ack.id()));
  }
}

void AutoPollClient::poll_thread() {
  while (active_) {
    try {
      // FIXME: return immediately if signaled to stop
      auto R =
          get_client(ClientType::POLL)
              ->poll_blocking(poll_timeout_, get_queue_id(ClientType::COMMAND));
      for (auto& r : R.result().results()) {
        results_.put(r.command_id(), r);
      }
    } catch (const yrclient::timeout& e) {
    } catch (const yrclient::system_error& e) {
      eprintf("internal error, likely cmd connection exit: {}", e.what());
      active_ = false;
    } catch (const std::exception& e) {
      eprintf("FATAL ERROR {}", e.what());
      active_ = false;
    }
  }
  dprintf("exiting");
}

ResultMap& AutoPollClient::results() { return results_; }

InstrumentationClient* AutoPollClient::get_client(const ClientType type) {
  return is_clients_[type].get();
}

u64 AutoPollClient::get_queue_id(ClientType t) const {
  return queue_ids_.at(t);
}
