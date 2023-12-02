#pragma once
#include "ra2yrproto/core.pb.h"

#include "command/command_manager.hpp"
#include "command/is_command.hpp"
#include "config.hpp"
#include "hook.hpp"
#include "process.hpp"
#include "types.h"
#include "utility/sync.hpp"
#include "websocket_server.hpp"

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

namespace ra2yrcpp {

// Forward declaration
class InstrumentationService;

/// Hook callback that provides access to InstrumentationService.
struct ISCallback : public hook::Callback {
  ISCallback();
  ~ISCallback() override;
  /// Add this callback to the given hook and assigns pointer to IService.
  void add_to_hook(hook::Hook* h, ra2yrcpp::InstrumentationService* I);

  ra2yrcpp::InstrumentationService* I;
};

using storage_t =
    std::map<std::string, std::unique_ptr<void, std::function<void(void*)>>>;
using ra2yrcpp::websocket_server::WebsocketServer;
using cmd_t = ra2yrcpp::command::iservice_cmd;
using cmd_manager_t = ra2yrcpp::command::CommandManager<cmd_t::data_t>;
using command_ptr_t = cmd_manager_t::command_ptr_t;
using hooks_t = std::map<std::uintptr_t, hook::Hook>;

class InstrumentationService {
 public:
  struct Options {
    WebsocketServer::Options server;
    bool no_init_hooks;
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

  ///
  /// Returns OS specific thread id's for all active client connections. Mostly
  /// useful during hooking to not suspend the connection threads.
  ///
  std::vector<process::thread_id_t> get_connection_threads();
  /// Create hook to given memory location
  /// @param name
  /// @param target target memory address
  /// @param code_length the amount of bytes to copy into target detour location
  void create_hook(const std::string& name, const std::uintptr_t target,
                   const std::size_t code_length);
  cmd_manager_t& cmd_manager();
  hooks_t& hooks();
  util::acquire_t<hooks_t> aq_hooks();
  // TODO(shmocz): separate storage class
  util::acquire_t<storage_t, std::recursive_mutex> aq_storage();

  template <typename T, typename... Args>
  void store_value(const std::string key, Args&&... args) {
    storage_[key] = std::unique_ptr<void, void (*)(void*)>(
        new T(std::forward<Args>(args)...),
        [](auto* d) { delete reinterpret_cast<T*>(d); });
  }

  /// Retrieve value from storage
  /// @param key target key
  /// @param acquire lock storage accessing it
  /// @return pointer to the storage object
  /// @exception std::out_of_range if value doesn't exist
  void* get_value(const std::string key, const bool acquire = true);
  storage_t& storage();
  const InstrumentationService::Options& opts() const;
  static ra2yrcpp::InstrumentationService* create(
      InstrumentationService::Options O,
      std::map<std::string, cmd_t::handler_t> commands,
      std::function<std::string(ra2yrcpp::InstrumentationService*)>
          on_shutdown = nullptr,
      std::function<void(InstrumentationService*)> extra_init = nullptr);
  ra2yrproto::Response process_request(const int socket_id, vecu8* bytes,
                                       bool* is_json);
  std::string on_shutdown();

 private:
  ra2yrproto::PollResults flush_results(
      const u64 queue_id, const duration_t delay = cfg::POLL_RESULTS_TIMEOUT);

  Options opts_;
  std::function<std::string(InstrumentationService*)> on_shutdown_;
  cmd_manager_t cmd_manager_;
  hooks_t hooks_;
  std::mutex mut_hooks_;
  storage_t storage_;
  std::recursive_mutex mut_storage_;
  std::unique_ptr<ra2yrcpp::asio_utils::IOService> io_service_;
  util::AtomicVariable<process::thread_id_t> io_service_tid_;

 public:
  std::unique_ptr<WebsocketServer> ws_server_;
};

std::tuple<command_ptr_t, ra2yrproto::RunCommandAck> handle_cmd(
    InstrumentationService* I, int queue_id, ra2yrproto::Command* cmd,
    bool discard_result = false);

const InstrumentationService::Options default_options{
    {cfg::SERVER_ADDRESS, cfg::SERVER_PORT, cfg::MAX_CLIENTS,
     cfg::ALLOWED_HOSTS_REGEX},
    true};

}  // namespace ra2yrcpp
