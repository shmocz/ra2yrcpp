#pragma once
#include "command/command.hpp"
#include "command/command_manager.hpp"
#include "config.hpp"
#include "hook.hpp"
#include "process.hpp"
#include "ra2yrproto/core.pb.h"
#include "server.hpp"
#include "types.h"
#include "utility/sync.hpp"
#include "websocket_server.hpp"

#include <google/protobuf/any.pb.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace connection {
class Connection;
}

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

struct ISArgs {
  yrclient::InstrumentationService* I;
  google::protobuf::Any M;
};

using deleter_t = std::function<void(void*)>;
using storage_val = std::unique_ptr<void, deleter_t>;
using storage_t = std::map<std::string, storage_val>;
using ws_proxy_t = ra2yrcpp::websocket_server::WebsocketProxy;

class InstrumentationService {
 public:
  struct IServiceOptions {
    unsigned max_clients;
    unsigned port;
    unsigned ws_port;
    std::string host;
    bool no_init_hooks;
  };

  InstrumentationService(
      IServiceOptions opt,
      std::function<std::string(InstrumentationService*)> on_shutdown,
      std::function<void(InstrumentationService*)> extra_init = nullptr);
  ~InstrumentationService();
  void add_command(std::string name, command::Command::handler_t fn);

  ///
  /// Returns OS specific thread id's for all active client connections. Mostly
  /// useful during hooking to not suspend the connection threads.
  ///
  std::vector<process::thread_id_t> get_connection_threads();
  void create_hook(std::string name, u8* target, const size_t code_length);
  void on_accept(connection::Connection* C);
  void on_close(connection::Connection* C);
  vecu8 on_receive_bytes(connection::Connection* C, vecu8* bytes);
  void on_send_bytes(connection::Connection* C, vecu8* bytes);
  command::CommandManager& cmd_manager();
  server::Server& server();
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
  const InstrumentationService::IServiceOptions& opts() const;
  std::vector<std::function<void(void*)>> stop_handlers_;
  // FIXME: dont expose
  static yrclient::InstrumentationService* create(
      InstrumentationService::IServiceOptions O,
      std::map<std::string, command::Command::handler_t>* commands,
      std::function<std::string(yrclient::InstrumentationService*)>
          on_shutdown = nullptr,
      std::function<void(InstrumentationService*)> extra_init = nullptr);

 private:
  ra2yrproto::PollResults flush_results(
      const u64 queue_id, const duration_t delay = cfg::POLL_RESULTS_TIMEOUT);
  ra2yrproto::Response process_request(connection::Connection* C, vecu8* bytes,
                                       bool* is_json);

  IServiceOptions opts_;
  command::CommandManager cmd_manager_;
  server::Server server_;
  std::map<u8*, hook::Hook> hooks_;
  std::mutex mut_hooks_;
  storage_t storage_;
  std::recursive_mutex mut_storage_;
  std::unique_ptr<ra2yrcpp::asio_utils::IOService> io_service_;
  util::AtomicVariable<process::thread_id_t> io_service_tid_;
  std::unique_ptr<ws_proxy_t> ws_proxy_object_;
};

}  // namespace yrclient
