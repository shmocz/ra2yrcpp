#include "instrumentation_client.hpp"

#include "protocol/protocol.hpp"

#include "client_connection.hpp"
#include "errors.hpp"
#include "protocol/helpers.hpp"

#include <fmt/core.h>

#include <stdexcept>

using namespace instrumentation_client;

InstrumentationClient::InstrumentationClient(
    std::shared_ptr<ra2yrcpp::connection::ClientConnection> conn)
    : conn_(conn) {}

ra2yrproto::PollResults InstrumentationClient::poll_blocking(
    const duration_t timeout, const u64 queue_id) {
  ra2yrproto::PollResults C;
  if (queue_id < (u64)-1) {
    auto* args = C.mutable_args();
    args->set_queue_id(queue_id);
    args->set_timeout((u64)timeout.count());
  }

  auto resp = send_command(C, ra2yrproto::POLL_BLOCKING);

  if (resp.code() == yrclient::RESPONSE_ERROR) {
    auto msg =
        ra2yrcpp::protocol::from_any<ra2yrproto::TextResponse>(resp.body());
    throw yrclient::system_error(msg.message());
  }
  return ra2yrcpp::protocol::from_any<ra2yrproto::PollResults>(resp.body());
}

void InstrumentationClient::send_data(const vecu8& data) {
  (void)conn_->send_data(data);
}

ra2yrproto::Response InstrumentationClient::send_message(const vecu8& data) {
  send_data(data);

  auto resp = conn_->read_data();
  if (resp.size() == 0U) {
    throw std::runtime_error("empty response, likely connection closed");
  }

  ra2yrproto::Response R;
  if (!R.ParseFromArray(resp.data(), resp.size())) {
    throw std::runtime_error(
        fmt::format("failed to parse response, size={}", resp.size()));
  }
  return R;
}

ra2yrproto::Response InstrumentationClient::send_message(
    const google::protobuf::Message& M) {
  auto data = yrclient::to_vecu8(M);
  return send_message(data);
}

ra2yrproto::Response InstrumentationClient::send_command(
    const google::protobuf::Message& cmd, ra2yrproto::CommandType type) {
  auto C = yrclient::create_command(cmd, type);
  return send_message(C);
}

ra2yrcpp::connection::ClientConnection* InstrumentationClient::connection() {
  return conn_.get();
}

void InstrumentationClient::connect() { conn_->connect(); }

void InstrumentationClient::disconnect() { conn_->stop(); }
