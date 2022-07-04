#include "instrumentation_client.hpp"

using namespace instrumentation_client;
using yrclient::to_json;

InstrumentationClient::InstrumentationClient(
    std::shared_ptr<connection::Connection> conn,
    const std::chrono::milliseconds poll_timeout,
    const std::chrono::milliseconds poll_rate)
    : conn_(conn), poll_timeout_(poll_timeout), poll_rate_(poll_rate) {}

InstrumentationClient::InstrumentationClient(
    const std::string host, const std::string port,
    const std::chrono::milliseconds poll_timeout,
    const std::chrono::milliseconds poll_rate)
    : InstrumentationClient(std::shared_ptr<connection::Connection>(
                                new connection::Connection(host, port)),
                            poll_timeout, poll_rate) {}

// TODO: get rid of this
yrclient::Response InstrumentationClient::poll(
    const std::chrono::milliseconds timeout) {
  (void)timeout;
  yrclient::Command C;
  return send_command(C, yrclient::POLL);
}

template <typename T>
auto unpack(const yrclient::Response& R) {
  T P;
  R.body().UnpackTo(&P);
  return P;
}

yrclient::PollResults InstrumentationClient::poll_blocking(
    const std::chrono::milliseconds timeout, const u64 queue_id) {
  yrclient::PollResults C;
  if (queue_id < (u64)-1) {
    auto* args = C.mutable_args();
    args->set_queue_id(queue_id);
    args->set_timeout((u64)timeout.count());
  }
  auto resp = send_command(C, yrclient::POLL_BLOCKING);
  if (resp.code() == yrclient::RESPONSE_ERROR) {
    auto msg = yrclient::from_any<yrclient::TextResponse>(resp.body());
    DPRINTF("%s\n", to_json(msg).c_str());
    throw yrclient::system_error(msg.message());
  }
  DPRINTF("resp=%s\n", to_json(resp).c_str());
  return yrclient::from_any<yrclient::PollResults>(resp.body());
}

size_t InstrumentationClient::send_data(const vecu8& data) {
  size_t sent = conn_->send_bytes(data);
  assert(sent == data.size());
  return sent;
}

yrclient::Response InstrumentationClient::send_message(const vecu8& data) {
  (void)send_data(data);
  auto resp = conn_->read_bytes();
  yrclient::Response R;
  R.ParseFromArray(resp.data(), resp.size());
  return R;
}

yrclient::Response InstrumentationClient::send_message(
    const google::protobuf::Message& M) {
  auto data = yrclient::to_vecu8(M);
  assert(!data.empty());
  return send_message(data);
}

yrclient::Response InstrumentationClient::send_command_old(
    std::string name, std::string args, yrclient::CommandType type) {
  yrclient::Command C;
  C.set_command_type(type);
  if (type == yrclient::CLIENT_COMMAND_OLD) {
    auto* CC = C.mutable_client_command_old();
    CC->set_name(name);
    CC->set_args(args);
  }
  // auto data = yrclient::to_vecu8(C);
  return send_message(C);
}

yrclient::Response InstrumentationClient::send_command(
    const google::protobuf::Message& cmd, yrclient::CommandType type) {
  yrclient::Command C;
  C.set_command_type(type);
  if (!C.mutable_command()->PackFrom(cmd)) {
    throw yrclient::general_error("Packging message failed");
  }
  return send_message(C);
}

yrclient::PollResults InstrumentationClient::poll_until(
    const std::chrono::milliseconds timeout,
    const std::chrono::milliseconds rate) {
  yrclient::PollResults P;
  auto f = [&]() {
    auto response = poll();
    if (!response.body().UnpackTo(&P)) {
      throw std::runtime_error("Could not unpack poll results");
    }
    return P.result().results().size() < 1;
  };
  util::call_until(timeout, rate, f);
  DPRINTF("size=%d\n", P.result().results().size());
  return P;
}

yrclient::CommandResult InstrumentationClient::run_one(
    const google::protobuf::Message& M) {
  auto r_ack = send_command(M, yrclient::CLIENT_COMMAND);
  if (r_ack.code() == yrclient::RESPONSE_ERROR) {
    throw std::runtime_error("ACK " + to_json(r_ack));
  }
  try {
    auto res = poll_until(poll_timeout_, poll_rate_);
    if (res.result().results_size() == 0) {
      return yrclient::CommandResult();
    }
    return res.result().results()[0];
  } catch (const std::runtime_error& e) {
    DPRINTF("broken connection %s\n", e.what());
    return yrclient::CommandResult();
  }
}

// FIXME: remove old code
std::string InstrumentationClient::shutdown() {
  auto r = send_command_old("shutdown", {}, yrclient::SHUTDOWN);
  yrclient::TextResponse T;
  r.body().UnpackTo(&T);
  return T.message();
}
