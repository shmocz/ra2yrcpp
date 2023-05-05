#pragma once
#include "protocol/protocol.hpp"

#include "async_map.hpp"
#include "client_utils.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "instrumentation_client.hpp"
#include "logging.hpp"
#include "network.hpp"
#include "websocket_connection.hpp"

#include <fmt/core.h>

#include <cstdint>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace multi_client {
namespace {
using namespace instrumentation_client;
using namespace async_map;

}  // namespace

enum class ClientType : i32 { COMMAND = 0, POLL = 1 };

using ResultMap = AsyncMap<ra2yrproto::CommandResult, u64>;

enum class CONNECTION_TYPE : int { TCP = 1, WEBSOCKET = 2 };

///
/// Client that uses two connections to fetch results in real time. One
/// connection issues command executions and the other polls the results.
///
class AutoPollClient {
 public:
  struct Options {
    std::string host;
    std::string port;
    duration_t poll_timeout;
    duration_t command_timeout;
    CONNECTION_TYPE ctype = CONNECTION_TYPE::TCP;
    void* io_service;
  };

  ///
  /// Establishes connection to InstrumentationService. Throws std::exception on
  /// failure. This function may (and probably will) block until succesful
  /// connection.
  ///
  AutoPollClient(const std::string host, const std::string port,
                 const duration_t poll_timeout = cfg::POLL_RESULTS_TIMEOUT,
                 const duration_t command_timeout = cfg::COMMAND_ACK_TIMEOUT,
                 CONNECTION_TYPE ctype = CONNECTION_TYPE::TCP,
                 void* io_service = nullptr);
  explicit AutoPollClient(AutoPollClient::Options o);
  ~AutoPollClient();
  ///
  /// Send command message with command client and poll results with poll client
  ///
  ra2yrproto::Response send_command(const google::protobuf::Message& cmd);
  static ra2yrproto::Response get_item();
  // Repeatedly executes blocking poll command on the backend, until stop signal
  // is given
  void poll_thread();
  ResultMap& results();
  InstrumentationClient* get_client(const ClientType type);
  u64 get_queue_id(ClientType t) const;

 private:
  std::string host_;
  std::string port_;
  const duration_t poll_timeout_;
  const duration_t command_timeout_;
  CONNECTION_TYPE ctype_;
  void* io_service_;
  std::atomic_bool active_;
  ResultMap results_;

  std::map<ClientType, std::unique_ptr<InstrumentationClient>> is_clients_;
  std::thread poll_thread_;
  std::map<ClientType, u64> queue_ids_;
};
}  // namespace multi_client
