#pragma once
#include "protocol/protocol.hpp"

#include "command/command.hpp"
#include "command/command_manager.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "hook.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "types.h"
#include "util_string.hpp"
#include "utility.h"
#include "utility/sync.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
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

template <typename T>
using aq_t = std::tuple<std::unique_lock<std::mutex>, T>;
using deleter_t = std::function<void(void*)>;
using storage_val = std::unique_ptr<void, deleter_t>;
using storage_t = std::map<std::string, storage_val>;

class InstrumentationService {
 public:
  struct IServiceOptions {
    unsigned max_clients;
    unsigned port;
    std::string host;

    IServiceOptions() : max_clients(0u), port(0u) {}
  };

  InstrumentationService(const unsigned int max_clients,
                         const unsigned int port,
                         std::function<std::string(InstrumentationService*)>
                             on_shutdown = nullptr);
  ~InstrumentationService();
  void add_command(std::string name, command::Command::handler_t fn,
                   command::Command::deleter_t deleter);

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
  aq_t<storage_t*> aq_storage();
  void store_value(const std::string key, vecu8* data);
  void store_value(const std::string key, void* data, deleter_t deleter);
  void* get_value(const std::string key, const bool acquire = true);
  // TODO: don't expose this
  std::function<std::string(InstrumentationService*)> on_shutdown_;

 private:
  ra2yrproto::Response flush_results(
      const u64 queue_id, const std::chrono::milliseconds delay = 1000ms);
  ra2yrproto::Response process_request(connection::Connection* C, vecu8* bytes);
  command::CommandManager cmd_manager_;
  server::Server server_;
  std::map<u8*, hook::Hook> hooks_;
  std::mutex mut_hooks_;
  storage_t storage_;
  std::mutex mut_storage_;
};

}  // namespace yrclient
