#pragma once
#include "ra2yrproto/core.pb.h"

#include "async_map.hpp"
#include "client_connection.hpp"
#include "config.hpp"
#include "instrumentation_client.hpp"
#include "types.h"
#include "utility/sync.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

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

namespace connection = ra2yrcpp::connection;

enum class ClientType : i32 { COMMAND = 0, POLL = 1 };

using ResultMap = AsyncMap<ra2yrproto::CommandResult, u64>;

///
/// Client that uses two connections to fetch results in real time. One
/// connection issues command executions and the other polls the results.
///
/// The destructor will automatically call shutdown() if it hasn't been done
/// already.
///
class AutoPollClient {
 public:
  struct Options {
    std::string host;
    std::string port;
    duration_t poll_timeout;
    duration_t command_timeout;
  };

  /// @exception std::exception on failed connection
  explicit AutoPollClient(
      std::shared_ptr<ra2yrcpp::asio_utils::IOService> io_service,
      AutoPollClient::Options o);
  ~AutoPollClient();

  /// Establishes connection to InstrumentationService.
  /// This function may (and probably will) block until succesful
  /// connection.
  ///
  /// @exception std::runtime_error if attempting to start already started
  /// object or on connection failure
  void start();
  void shutdown();
  ///
  /// Send command message with command client and poll results with poll client
  ///
  ra2yrproto::Response send_command(const gpb::Message& cmd);
  static ra2yrproto::Response get_item();

  ResultMap& results();
  /// Get the providied client type initialized by start()
  ///
  /// @exception std::out_of_range if the client  doesn't exist.
  InstrumentationClient* get_client(const ClientType type);
  u64 get_queue_id(ClientType t) const;

 private:
  const Options opt_;
  std::shared_ptr<ra2yrcpp::asio_utils::IOService> io_service_;
  util::AtomicVariable<connection::State, std::recursive_mutex> state_;
  std::atomic_bool poll_thread_active_;
  ResultMap results_;

  std::map<ClientType, std::unique_ptr<InstrumentationClient>> is_clients_;
  std::thread poll_thread_;
  std::map<ClientType, u64> queue_ids_;

  // Repeatedly executes blocking poll command on the backend, until stop signal
  // is given
  void poll_thread();
};

const AutoPollClient::Options default_options = {
    cfg::SERVER_ADDRESS, std::to_string(cfg::SERVER_PORT),
    cfg::POLL_RESULTS_TIMEOUT, cfg::COMMAND_ACK_TIMEOUT};

}  // namespace multi_client
