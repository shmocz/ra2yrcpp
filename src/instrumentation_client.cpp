#include "instrumentation_client.hpp"

using namespace instrumentation_client;

InstrumentationClient::InstrumentationClient(
    connection::Connection* conn, const std::chrono::milliseconds poll_timeout,
    const std::chrono::milliseconds poll_rate)
    : conn_(conn), poll_timeout_(poll_timeout), poll_rate_(poll_rate) {}

yrclient::Response InstrumentationClient::poll() {
  yrclient::Command C;
  C.set_command_type(yrclient::POLL);
  auto R = send_command("", "", yrclient::POLL);
  return R;
}

yrclient::CommandPollResult instrumentation_client::parse_poll(
    const yrclient::Response& R) {
  yrclient::CommandPollResult P;
  R.body().UnpackTo(&P);
  return P;
}

std::vector<std::string> instrumentation_client::get_poll_results(
    const yrclient::Response& R) {
  std::vector<std::string> res;
  auto P = parse_poll(R);
  for (auto& c : P.results()) {
    res.push_back(c.data());
  }
  return res;
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

yrclient::Response InstrumentationClient::send_command(
    std::string name, std::string args, yrclient::CommandType type) {
  yrclient::Command C;
  C.set_command_type(type);
  if (type == yrclient::CLIENT_COMMAND) {
    auto* CC = C.mutable_client_command();
    CC->set_name(name);
    CC->set_args(args);
  }
  auto data = yrclient::to_vecu8(C);
  assert(!data.empty());
  auto R = send_message(data);
  return R;
}

yrclient::Response InstrumentationClient::run_command(std::string name,
                                                      std::string args) {
  auto r_ack = send_command(name, args);
  auto r_body = poll();
  return r_body;
}

yrclient::CommandPollResult InstrumentationClient::poll_until(
    const std::chrono::milliseconds timeout,
    const std::chrono::milliseconds rate) {
  yrclient::CommandPollResult P;
  auto deadline = util::current_time() + timeout;
  while (P.results().size() < 1 && util::current_time() < deadline) {
    auto R = poll();
    P = parse_poll(R);
    util::sleep_ms(rate);
  }
  return P;
}

std::string InstrumentationClient::run_one(std::string name, std::string args) {
  auto r_ack = send_command(name, args);
  auto res = poll_until(poll_timeout_, poll_rate_);
  auto err = [&](std::string args) {
    throw std::runtime_error(name + " " + args);
  };
  if (res.results().size() < 1) {
    err("No results in queue");
  }
  if (res.results().size() > 1) {
    err("Excess entries in result queue");
  }
  auto res0 = res.results()[0];
  if (res0.result_code() == yrclient::RESPONSE_ERROR) {
    err("Command failed");
  }
  DPRINTF("name=%s, args=%s, result=%s\n", name.c_str(), args.c_str(),
          res0.data().c_str());
  return res0.data();
}

std::string InstrumentationClient::run_one(std::string name,
                                           std::vector<std::string> args) {
  return run_one(name, yrclient::join_string(args));
}

std::string InstrumentationClient::shutdown() {
  auto r = send_command("shutdown", {}, yrclient::SHUTDOWN);
  yrclient::TextResponse T;
  r.body().UnpackTo(&T);
  return T.message();
}
