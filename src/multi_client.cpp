#include "multi_client.hpp"

using namespace multi_client;

AutoPollClient::AutoPollClient(const std::string host, const std::string port,
                               const std::chrono::milliseconds poll_timeout,
                               const std::chrono::milliseconds command_timeout)
    : host_(host),
      port_(port),
      poll_timeout_(poll_timeout),
      command_timeout_(command_timeout),
      active_(true) {
  static constexpr std::array<ClientType, 2> t = {ClientType::COMMAND,
                                                  ClientType::POLL};
  for (auto i : t) {
    auto* c = new connection::Connection(host_, port_);
    network::set_io_timeout(c->socket(), 10000);
    is_clients_[i] = std::make_unique<InstrumentationClient>(
        std::shared_ptr<connection::Connection>(c));
  }
  poll_thread_ = std::thread([this]() { poll_thread(); });
}

AutoPollClient::~AutoPollClient() {
  active_ = false;
  poll_thread_.join();
}

yrclient::Response AutoPollClient::send_command(
    const google::protobuf::Message& cmd) {
  // Send command
  auto resp = get_client(ClientType::COMMAND)
                  ->send_command(cmd, yrclient::CommandType::CLIENT_COMMAND);
  auto ack = yrclient::from_any<yrclient::RunCommandAck>(resp.body());
  // FIXME: ugly hack to set result queue id
  if (queue_id_ == (u64)-1) {
    queue_id_ = ack.queue_id();
    yrclient::CommandResult C;
    C.set_command_id(-1);
    results().put((u64)-1, C);
  }
  // Wait until item found from polled messages
  dprintf("ack={}", ack.id());
  try {
    // FIXME: signal if poll_thread dies
    auto poll_res = results().get(ack.id(), command_timeout_);
    return make_response(yrclient::RESPONSE_OK, poll_res);
  } catch (const std::runtime_error& e) {
    throw yrclient::general_error(fmt::format(
        "timeout after {}ms, key={}", command_timeout_.count(), ack.id()));
  }
}

void AutoPollClient::poll_thread() {
  // wait for queue id to be set up
  auto item = results().get((u64)-1, command_timeout_);
  if (item.command_id() != -1) {
    throw std::runtime_error("couldn't get command queue id");
  }
  while (active_) {
    try {
      auto R =
          get_client(ClientType::POLL)->poll_blocking(poll_timeout_, queue_id_);
      for (auto& r : R.result().results()) {
        results_.put(r.command_id(), r);
      }
    } catch (const yrclient::timeout& e) {
    } catch (const yrclient::system_error& e) {
      eprintf("internal error, likely cmd connection exit: {}", e.what());
      active_ = false;
    }
  }
  dprintf("exiting");
}

ResultMap& AutoPollClient::results() { return results_; }

InstrumentationClient* AutoPollClient::get_client(const ClientType type) {
  return is_clients_[type].get();
}

u64 AutoPollClient::queue_id() const { return queue_id_; }
