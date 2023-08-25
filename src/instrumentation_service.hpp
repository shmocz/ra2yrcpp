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

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

namespace yrclient {

// Forward declaration
class InstrumentationService;

/// Hook callback that provides access to InstrumentationService.
struct ISCallback : public hook::Callback {
  ISCallback();
  ~ISCallback() override;
  /// Add this callback to the given hook and assigns pointer to IService.
  void add_to_hook(hook::Hook* h, yrclient::InstrumentationService* I);

  yrclient::InstrumentationService* I;
};

using deleter_t = std::function<void(void*)>;
using storage_val = std::unique_ptr<void, deleter_t>;
using storage_t = std::map<std::string, storage_val>;
using ra2yrcpp::websocket_server::WebsocketServer;
using cmd_t = ra2yrcpp::command::iservice_cmd;
using cmd_manager_t = ra2yrcpp::command::CommandManager<cmd_t::data_t>;
using command_ptr_t = cmd_manager_t::command_ptr_t;

class InstrumentationService {
 public:
  struct Options {
    WebsocketServer::Options server;
    bool no_init_hooks;
  };

  InstrumentationService(
      Options opt,
      std::function<std::string(InstrumentationService*)> on_shutdown,
      std::function<void(InstrumentationService*)> extra_init = nullptr);
  ~InstrumentationService();
  void add_command(std::string name, cmd_t::handler_t fn);

  ///
  /// Returns OS specific thread id's for all active client connections. Mostly
  /// useful during hooking to not suspend the connection threads.
  ///
  std::vector<process::thread_id_t> get_connection_threads();
  void create_hook(std::string name, u8* target, const std::size_t code_length);
  cmd_manager_t& cmd_manager();
  std::map<u8*, hook::Hook>& hooks();
  // TODO: separate storage class
  util::acquire_t<std::map<u8*, hook::Hook>*> aq_hooks();
  util::acquire_t<storage_t*, std::recursive_mutex> aq_storage();
  void store_value(const std::string key,
                   std::unique_ptr<void, void (*)(void*)> d);
  void store_value(const std::string key, vecu8* data);
  void store_value(const std::string key, void* data, deleter_t deleter);
  void* get_value(const std::string key, const bool acquire = true);
  // TODO: don't expose this
  std::function<std::string(InstrumentationService*)> on_shutdown_;
  storage_t& storage();
  const InstrumentationService::Options& opts() const;
  std::vector<std::function<void(void*)>> stop_handlers_;
  // FIXME: dont expose
  static yrclient::InstrumentationService* create(
      InstrumentationService::Options O,
      std::map<std::string, cmd_t::handler_t> commands,
      std::function<std::string(yrclient::InstrumentationService*)>
          on_shutdown = nullptr,
      std::function<void(InstrumentationService*)> extra_init = nullptr);
  ra2yrproto::Response process_request(const int socket_id, vecu8* bytes,
                                       bool* is_json);

 private:
  ra2yrproto::PollResults flush_results(
      const u64 queue_id, const duration_t delay = cfg::POLL_RESULTS_TIMEOUT);

  Options opts_;
  cmd_manager_t cmd_manager_;
  std::map<u8*, hook::Hook> hooks_;
  std::mutex mut_hooks_;
  storage_t storage_;
  std::recursive_mutex mut_storage_;
  std::unique_ptr<ra2yrcpp::asio_utils::IOService> io_service_;
  util::AtomicVariable<process::thread_id_t> io_service_tid_;

 public:
  std::unique_ptr<WebsocketServer> ws_server_;
};

std::tuple<command_ptr_t, ra2yrproto::RunCommandAck> handle_cmd(
    InstrumentationService* I, const int queue_id, ra2yrproto::Command* cmd,
    const bool discard_result = false);

const InstrumentationService::Options default_options{
    {cfg::SERVER_ADDRESS, cfg::SERVER_PORT, cfg::MAX_CLIENTS,
     cfg::ALLOWED_HOSTS_REGEX},
    true};

}  // namespace yrclient
