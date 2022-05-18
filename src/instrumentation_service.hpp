#pragma once
#include "command_manager.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "errors.hpp"
// See issue #1. hook.hpp includes xbyak so needs to be after protocol
#include "protocol/protocol.hpp"
#include "hook.hpp"
#include "server.hpp"
#include "types.h"
#include "util_string.hpp"
#include <algorithm>
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

// Forward declaration
class InstrumentationService;

struct IServiceArgs {
  InstrumentationService* I;
  std::string* args;
  void* result;
};

using IServiceCommand = std::function<std::unique_ptr<vecu8>(IServiceArgs)>;

class InstrumentationService {
 public:
  using deleter_t = std::function<void(void*)>;
  using storage_val = std::unique_ptr<void, deleter_t>;
  InstrumentationService(const unsigned int max_clients,
                         const unsigned int port,
                         std::function<std::string(InstrumentationService*)>
                             on_shutdown = nullptr);
  ~InstrumentationService();
  void add_command(std::string name, IServiceCommand cmd);
  void remove_command();
  void install_hook_callback();
  std::vector<process::thread_id_t> get_connection_threads();
  void create_hook(std::string name, u8* target, const size_t code_length);
  void start_server(const char* bind, const unsigned int port,
                    const size_t max_clients);
  void stop_server();
  void on_accept(connection::Connection* C);
  void on_close(connection::Connection* C);
  vecu8 on_receive_bytes(connection::Connection* C, vecu8* bytes);
  void on_send_bytes(connection::Connection* C, vecu8* bytes);
  void on_accept_connection(connection::Connection* C);
  command_manager::CommandManager& cmd_manager();
  server::Server& server();
  std::map<u8*, hook::Hook>& hooks();
  std::tuple<std::unique_lock<std::mutex>, std::map<u8*, hook::Hook>*>
  aq_hooks();
  void store_value(const std::string key, vecu8* data);
  void store_value(const std::string key, void* data, deleter_t deleter);
  void* get_value(const std::string key);
  void remove_value(const std::string key);

 private:
  void add_builtin_commands();
  yrclient::Response flush_results(const size_t id);
  yrclient::Response process_request(connection::Connection* C, vecu8* bytes);
  command_manager::CommandManager cmd_manager_;
  server::Server server_;
  std::map<u8*, hook::Hook> hooks_;
  std::mutex mut_hooks_;
  std::map<std::string, storage_val> storage_;
  std::mutex mut_storage_;
  std::function<std::string(InstrumentationService*)> on_shutdown_;
};

std::tuple<yrclient::InstrumentationService*, std::vector<std::string>, void*>
get_args(yrclient::IServiceArgs args);

};  // namespace yrclient
