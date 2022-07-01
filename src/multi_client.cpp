#include "multi_client.hpp"

#include "errors.hpp"
#include "network.hpp"
#include "protocol/protocol.hpp"

#include <stdexcept>
using namespace multi_client;

AutoPollClient::AutoPollClient(const std::string host, const std::string port,
                               const std::chrono::milliseconds poll_timeout,
                               const std::chrono::milliseconds command_timeout)
    : host_(host),
      port_(port),
      poll_timeout_(poll_timeout),
      command_timeout_(command_timeout) {
  active_ = true;
  static constexpr std::array<ClientType, 2> t = {ClientType::COMMAND,
                                                  ClientType::POLL};
  for (auto i : t) {
    auto* c = new connection::Connection(host_, port_);
    network::set_io_timeout(c->socket(), 5000);
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
  auto resp =
      get_client(ClientType::COMMAND)
          ->send_command(cmd, yrclient::CommandType::CLIENT_COMMAND_NEW);
  auto ack = yrclient::from_any<yrclient::RunCommandAck>(resp.body());
  // Wait until item found from polled messages
  DPRINTF("ack=%lld\n", ack.id());
  auto poll_res = results().get(ack.id(), command_timeout_);
  return make_response(yrclient::RESPONSE_OK, poll_res);
}

static std::vector<u64> get_queue_ids(InstrumentationClient* I) {
  std::vector<u64> r;
  auto res = client_utils::run(yrclient::GetSystemState(), I);
  for (auto& q : res.state().queues()) {
    r.push_back(q.queue_id());
  }
  return r;
}

void AutoPollClient::poll_thread() {
  std::vector<u64> queue_ids;
  util::call_until(4000ms, 500ms, [&]() {
    queue_ids = get_queue_ids(get_client(ClientType::POLL));
    return queue_ids.size() < 2;
  });
  while (active_) {
    try {
      auto R = get_client(ClientType::POLL)
                   ->poll_blocking(poll_timeout_, queue_ids[0]);
      for (auto& r : R.result().results()) {
        results_.put(r.command_id(), r);
      }
    } catch (const yrclient::timeout& e) {
    } catch (const yrclient::system_error& e) {
      DPRINTF("internal error, likely cmd connection exit: %s\n", e.what());
      active_ = false;
    }
  }
  DPRINTF("exiting\n");
}

ResultMap& AutoPollClient::results() { return results_; }

InstrumentationClient* AutoPollClient::get_client(const ClientType type) {
  return is_clients_[type].get();
}
