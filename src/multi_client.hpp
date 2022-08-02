#pragma once
#include "protocol/protocol.hpp"

#include "async_map.hpp"
#include "client_utils.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "instrumentation_client.hpp"
#include "network.hpp"

#include <cstdint>

#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace multi_client {
namespace {
using namespace instrumentation_client;
using namespace std::chrono_literals;
using namespace async_map;

}  // namespace

enum class ClientType : i32 { COMMAND = 0, POLL = 1 };

using ResultMap = AsyncMap<yrclient::CommandResult, u64>;

class AutoPollClient {
 public:
  AutoPollClient(const std::string host, const std::string port,
                 const std::chrono::milliseconds poll_timeout = 1000ms,
                 const std::chrono::milliseconds command_timeout = 250ms);
  ~AutoPollClient();
  ///
  /// Send command message with command client and poll results with poll client
  ///
  yrclient::Response send_command(const google::protobuf::Message& cmd);
  static yrclient::Response get_item();
  // Repeatedly executes blocking poll command on the backend, until stop signal
  // is given
  void poll_thread();
  ResultMap& results();
  InstrumentationClient* get_client(const ClientType type);

 private:
  std::string host_;
  std::string port_;
  const std::chrono::milliseconds poll_timeout_;
  const std::chrono::milliseconds command_timeout_;

  std::map<ClientType, std::unique_ptr<connection::Connection>> conns_;
  std::map<ClientType, std::unique_ptr<InstrumentationClient>> is_clients_;
  std::map<ClientType, u64> queue_ids_;
  std::thread poll_thread_;
  ResultMap results_;
  std::atomic_bool active_;
  const u64 queue_id_{(u64)-1};
};
}  // namespace multi_client
