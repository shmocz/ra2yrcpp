#pragma once
#include "async_map.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "instrumentation_client.hpp"
#include "ra2yrproto/core.pb.h"
#include "types.h"
#include "utility/sync.hpp"

#include <map>
#include <memory>
#include <string>
#include <thread>

namespace google {
namespace protobuf {
class Message;
}
}  // namespace google

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
  /// Establishes connection to InstrumentationService.
  /// This function may (and probably will) block until succesful
  /// connection.
  /// TODO: don't establish connection here, write a separate method
  /// @throws std::exception on failed connection
  /// @param host Destination address/hostname.
  /// @param port Destination port.
  /// @param poll_timeout How long the poll thread should wait for results.
  /// @param command_timeout How long the connection thread should wait for ACK
  /// from the service.
  /// @param ctype The type of connection to establish.
  /// @param io_service (For WebSocket connections only). A pointer to an
  /// external IO service object.
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
  util::AtomicVariable<connection::State> state_;
  ResultMap results_;

  std::map<ClientType, std::unique_ptr<InstrumentationClient>> is_clients_;
  std::thread poll_thread_;
  std::map<ClientType, u64> queue_ids_;

  // Repeatedly executes blocking poll command on the backend, until stop signal
  // is given
  void poll_thread();
};
}  // namespace multi_client
