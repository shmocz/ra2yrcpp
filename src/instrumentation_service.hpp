#pragma once
#include "ra2yrproto/core.pb.h"

#include "command/command_manager.hpp"
#include "command/is_command.hpp"
#include "constants.hpp"
#include "hook.hpp"
#include "process.hpp"
#include "types.h"
#include "utility/sync.hpp"
#include "websocket_server.hpp"

#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

namespace ra2yrcpp {

// Forward declaration
class InstrumentationService;

// TODO(shmocz): Deprecate storage because it's largely unused.
using storage_t =
    std::map<std::string, std::unique_ptr<void, std::function<void(void*)>>>;
using ra2yrcpp::websocket_server::WebsocketServer;
using cmd_t = ra2yrcpp::command::iservice_cmd;
using cmd_manager_t = ra2yrcpp::command::CommandManager<cmd_t::data_t>;
using command_ptr_t = cmd_manager_t::command_ptr_t;
using command_hdl_t = command_ptr_t::weak_type;

class InstrumentationService {
 public:
  struct Options {
    WebsocketServer::Options server;
  };

  /// @param opt options
  /// @param on_shutdown Callback invoked upon SHUTDOWN command. Used to e.g.
  /// signal the Context object to delete the main service. Currently not
  /// utilized in practice.
  /// @param extra_init Function to be invoked right after starting the command
  /// manager.
  InstrumentationService(
      Options opt,
      std::function<std::string(InstrumentationService*)> on_shutdown,
      std::function<void(InstrumentationService*)> extra_init = nullptr);
  ~InstrumentationService();

  cmd_manager_t& cmd_manager();
  // TODO(shmocz): separate storage class
  util::acquire_t<storage_t, std::recursive_mutex> aq_storage();

  template <typename T, typename... Args>
  void store_value(std::string key, Args&&... args) {
    storage_[key] = std::unique_ptr<void, void (*)(void*)>(
        new T(std::forward<Args>(args)...),
        [](auto* d) { delete reinterpret_cast<T*>(d); });
  }

  /// Retrieve value from storage
  /// @param key target key
  /// @param acquire lock storage accessing it
  /// @return pointer to the storage object
  /// @exception std::out_of_range if value doesn't exist
  void* get_value(std::string key, bool acquire = true);
  const InstrumentationService::Options& opts() const;
  static ra2yrcpp::InstrumentationService* create(
      InstrumentationService::Options O,
      std::map<std::string, cmd_t::handler_t> commands,
      std::function<std::string(ra2yrcpp::InstrumentationService*)>
          on_shutdown = nullptr,
      std::function<void(InstrumentationService*)> extra_init = nullptr);
  ra2yrproto::Response process_request(int socket_id, vecu8* bytes,
                                       bool* is_json);
  std::string on_shutdown();

 private:
  ra2yrproto::PollResults flush_results(
      u64 queue_id, duration_t delay = cfg::POLL_RESULTS_TIMEOUT);

  Options opts_;
  std::function<std::string(InstrumentationService*)> on_shutdown_;
  cmd_manager_t cmd_manager_;
  storage_t storage_;
  std::recursive_mutex mut_storage_;
  std::unique_ptr<ra2yrcpp::asio_utils::IOService> io_service_;
  util::AtomicVariable<process::thread_id_t> io_service_tid_;

 public:
  std::unique_ptr<WebsocketServer> ws_server_;
};

const InstrumentationService::Options default_options{
    {cfg::SERVER_ADDRESS, cfg::SERVER_PORT, cfg::MAX_CLIENTS,
     cfg::ALLOWED_HOSTS_REGEX}};

}  // namespace ra2yrcpp
