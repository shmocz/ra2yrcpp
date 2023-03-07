#pragma once
#include "protocol/protocol.hpp"

#include "command/command.hpp"
#include "command/command_manager.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "hook.hpp"
#include "logging.hpp"
#include "process.hpp"
#include "server.hpp"
#include "types.h"
#include "util_string.hpp"
#include "utility.h"
#include "utility/sync.hpp"
#include "websocket_server.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace yrclient {

namespace {
using namespace std::chrono_literals;
};

// Forward declaration
class InstrumentationService;

// Hook callback that provides access to InstrumentationService.
// TODO(shmocz): get rid of polymorphism
struct ISCallback {
  ISCallback();
  // Callback entry, whose address is stored into Hook's HookCallback object.
  // Performs necessary setup logic for IService access.
  void call(hook::Hook* h, void* data, X86Regs* state);
  // Subclasses implement their callback logic by overriding this.
  virtual void do_call(yrclient::InstrumentationService* I) = 0;
  ISCallback(const ISCallback&) = delete;
  ISCallback& operator=(const ISCallback&) = delete;
  virtual ~ISCallback() = default;
  // Callbacks name. Duplicate callbacks will not be added into Hook.
  virtual std::string name();
  // Target hook name.
  virtual std::string target();
  yrclient::InstrumentationService* I;
  X86Regs* cpu_state;
};

struct ISArgs {
  yrclient::InstrumentationService* I;
  google::protobuf::Any M;
};

template <typename T, typename MutexT = std::mutex>
using aq_t = std::tuple<std::unique_lock<MutexT>, T>;
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
      std::function<std::string(InstrumentationService*)> on_shutdown);
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
  void on_accept_connection(connection::Connection* C);
  command::CommandManager& cmd_manager();
  server::Server& server();
  std::map<u8*, hook::Hook>& hooks();
  // TODO: separate storage class
  aq_t<std::map<u8*, hook::Hook>*> aq_hooks();
  aq_t<storage_t*, std::recursive_mutex> aq_storage();
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
          on_shutdown = nullptr);

 private:
  ra2yrproto::PollResults flush_results(
      const u64 queue_id, const std::chrono::milliseconds delay = 1000ms);
  ra2yrproto::Response process_request(connection::Connection* C, vecu8* bytes);

  IServiceOptions opts_;
  command::CommandManager cmd_manager_;
  server::Server server_;
  std::map<u8*, hook::Hook> hooks_;
  std::mutex mut_hooks_;
  storage_t storage_;
  std::recursive_mutex mut_storage_;
  ra2yrcpp::websocket_server::IOService io_service_;
  util::AtomicVariable<process::thread_id_t> io_service_tid_;
  ws_proxy_t ws_proxy_object_;
};

}  // namespace yrclient
