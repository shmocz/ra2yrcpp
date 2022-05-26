#include "instrumentation_client.hpp"
#include "errors.hpp"
#include "protocol/protocol.hpp"

using namespace instrumentation_client;
using yrclient::to_json;

InstrumentationClient::InstrumentationClient(
    connection::Connection* conn, const std::chrono::milliseconds poll_timeout,
    const std::chrono::milliseconds poll_rate)
    : conn_(conn), poll_timeout_(poll_timeout), poll_rate_(poll_rate) {}

yrclient::Response InstrumentationClient::poll() {
  yrclient::Command C;
  return send_command(C, yrclient::POLL_NEW);
}

template <typename T>
auto unpack(const yrclient::Response& R) {
  T P;
  R.body().UnpackTo(&P);
  return P;
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
  if (type == yrclient::CLIENT_COMMAND) {
    auto* CC = C.mutable_client_command();
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
  if (!C.mutable_command_new()->PackFrom(cmd)) {
    throw yrclient::general_error("Packging message failed");
  }
  return send_message(C);
}

yrclient::NewCommandPollResult InstrumentationClient::poll_until(
    const std::chrono::milliseconds timeout,
    const std::chrono::milliseconds rate) {
  yrclient::NewCommandPollResult P;
  auto f = [&]() {
    auto R = poll();
    R.body().UnpackTo(&P);
    return P.results().size() < 1;
  };
  util::call_until(timeout, rate, f);
  return P;
}

yrclient::NewResult InstrumentationClient::run_one(
    const google::protobuf::Message& M) {
  auto r_ack = send_command(M, yrclient::CLIENT_COMMAND_NEW);
  // use reflection to set the command type
  if (r_ack.code() == yrclient::RESPONSE_ERROR) {
    throw std::runtime_error("ACK " + to_json(r_ack));
  }
  auto res = poll_until(poll_timeout_, poll_rate_);
  auto res0 = res.results()[0];
  yrclient::NewResult r;
  res0.UnpackTo(&r);
  return r;
}

std::string InstrumentationClient::shutdown() {
  auto r = send_command_old("shutdown", {}, yrclient::SHUTDOWN);
  yrclient::TextResponse T;
  r.body().UnpackTo(&T);
  return T.message();
}
