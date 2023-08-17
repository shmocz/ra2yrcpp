#pragma once

#include "ra2yrproto/core.pb.h"

#include "instrumentation_client.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "types.h"
#include "utility/time.hpp"

#include <fmt/chrono.h>

#include <chrono>
#include <exception>

namespace client_utils {

namespace {
using namespace std::chrono_literals;
};

inline ra2yrproto::PollResults poll_until(
    instrumentation_client::InstrumentationClient* client,
    const duration_t timeout = 5.0s) {
  ra2yrproto::PollResults P;
  ra2yrproto::Response response;

  constexpr int max_tries = 4;
  constexpr int retry_ms = 1000;

  for (int i = 1; i <= max_tries; i++) {
    try {
      P = client->poll_blocking(timeout, 0u);
      break;
    } catch (const std::exception& e) {
      eprintf("poll failed: \"{}\", retry after {} ms, try {}/{}", e.what(),
              retry_ms, i, max_tries);
      util::sleep_ms(retry_ms);
    }
  }

  dprintf("size={}", P.result().results().size());
  return P;
}

inline ra2yrproto::CommandResult run_one(
    const google::protobuf::Message& M,
    instrumentation_client::InstrumentationClient* client,
    const duration_t poll_timeout = 5.0s) {
  auto r_ack = client->send_command(M, ra2yrproto::CLIENT_COMMAND);
  if (r_ack.code() == ra2yrproto::ResponseCode::ERROR) {
    throw std::runtime_error("ACK " + ra2yrcpp::protocol::to_json(r_ack));
  }
  try {
    auto res = poll_until(client, poll_timeout);
    if (res.result().results_size() == 0) {
      return ra2yrproto::CommandResult();
    }
    return res.result().results()[0];
  } catch (const std::exception& e) {
    eprintf("broken connection {}", e.what());
    return ra2yrproto::CommandResult();
  }
}

template <typename T>
inline auto run(const T& cmd,
                instrumentation_client::InstrumentationClient* client) {
  try {
    auto r = run_one(cmd, client);
    return ra2yrcpp::protocol::from_any<T>(r.result());
  } catch (const std::exception& e) {
    dprintf("failed to run: {}", e.what());
    throw;
  }
}

struct CommandSender {
  using fn_t = std::function<ra2yrproto::CommandResult(
      const google::protobuf::Message&)>;

  explicit CommandSender(fn_t fn) : fn(fn) {}

  template <typename T>
  auto run(const T& cmd) {
    auto r = fn(cmd);
    return ra2yrcpp::protocol::from_any<T>(r.result());
  }

  fn_t fn;
};

}  // namespace client_utils
