#include "instrumentation_client.hpp"

using namespace instrumentation_client;
using yrclient::to_json;

InstrumentationClient::InstrumentationClient(
    std::shared_ptr<connection::ClientConnection> conn,
    const std::chrono::milliseconds poll_timeout)
    : conn_(conn), poll_timeout_(poll_timeout) {}

// TODO: get rid of this
ra2yrproto::Response InstrumentationClient::poll(
    const std::chrono::milliseconds timeout) {
  (void)timeout;
  ra2yrproto::Command C;
  return send_command(C, ra2yrproto::POLL);
}

template <typename T>
auto unpack(const ra2yrproto::Response& R) {
  T P;
  R.body().UnpackTo(&P);
  return P;
}

ra2yrproto::PollResults InstrumentationClient::poll_blocking(
    const std::chrono::milliseconds timeout, const u64 queue_id) {
  ra2yrproto::PollResults C;
  if (queue_id < (u64)-1) {
    auto* args = C.mutable_args();
    args->set_queue_id(queue_id);
    args->set_timeout((u64)timeout.count());
  }

  auto resp = send_command(C, ra2yrproto::POLL_BLOCKING);

  if (resp.code() == yrclient::RESPONSE_ERROR) {
    auto msg = yrclient::from_any<ra2yrproto::TextResponse>(resp.body());
    dprintf("{}", to_json(msg).c_str());
    throw yrclient::system_error(msg.message());
  }
  return yrclient::from_any<ra2yrproto::PollResults>(resp.body());
}

size_t InstrumentationClient::send_data(const vecu8& data) {
  (void)conn_->send_data(data);
  return data.size();
}

ra2yrproto::Response InstrumentationClient::send_message(const vecu8& data) {
  if (send_data(data) != data.size()) {
    throw std::runtime_error("sent data size mismatch");
  }

  auto resp = conn_->read_data();
  if (resp.size() == 0U) {
    throw std::runtime_error("empty response, likely connection closed");
  }

  ra2yrproto::Response R;
  R.ParseFromArray(resp.data(), resp.size());
  return R;
}

ra2yrproto::Response InstrumentationClient::send_message(
    const google::protobuf::Message& M) {
  auto data = yrclient::to_vecu8(M);
  assert(!data.empty());
  return send_message(data);
}

ra2yrproto::Response InstrumentationClient::send_command_old(
    std::string name, std::string args, ra2yrproto::CommandType type) {
  ra2yrproto::Command C;
  C.set_command_type(type);
  if (type == ra2yrproto::CLIENT_COMMAND_OLD) {
    auto* CC = C.mutable_client_command_old();
    CC->set_name(name);
    CC->set_args(args);
  }
  // auto data = ra2yrproto::to_vecu8(C);
  return send_message(C);
}

ra2yrproto::Response InstrumentationClient::send_command(
    const google::protobuf::Message& cmd, ra2yrproto::CommandType type) {
  ra2yrproto::Command C;
  C.set_command_type(type);
  if (!C.mutable_command()->PackFrom(cmd)) {
    throw yrclient::general_error("Packging message failed");
  }
  return send_message(C);
}

ra2yrproto::PollResults InstrumentationClient::poll_until(
    const std::chrono::milliseconds timeout) {
  ra2yrproto::PollResults P;
  ra2yrproto::Response response;
  P = poll_blocking(timeout, 0u);

  dprintf("size={}", P.result().results().size());
  return P;
}

ra2yrproto::CommandResult InstrumentationClient::run_one(
    const google::protobuf::Message& M) {
  auto r_ack = send_command(M, ra2yrproto::CLIENT_COMMAND);
  if (r_ack.code() == yrclient::RESPONSE_ERROR) {
    throw std::runtime_error("ACK " + to_json(r_ack));
  }
  try {
    auto res = poll_until(poll_timeout_);
    if (res.result().results_size() == 0) {
      return ra2yrproto::CommandResult();
    }
    return res.result().results()[0];
  } catch (const std::runtime_error& e) {
    eprintf("broken connection {}", e.what());
    return ra2yrproto::CommandResult();
  }
}

// FIXME: remove old code
std::string InstrumentationClient::shutdown() {
  auto r = send_command_old("shutdown", {}, ra2yrproto::SHUTDOWN);
  ra2yrproto::TextResponse T;
  r.body().UnpackTo(&T);
  return T.message();
}

connection::ClientConnection* InstrumentationClient::connection() {
  return conn_.get();
}
