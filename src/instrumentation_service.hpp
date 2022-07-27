#pragma once
#include "protocol/protocol.hpp"

#include "command/command_manager.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "hook.hpp"
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

struct IServiceArgs {
  InstrumentationService* I;
  // currently it's a space separated string, but it could be a protobuf
  // message. In case of latter, it should have the fields "args" and "result"
  // defined. result field is set to null, and populated by the command when
  // response is sent back
  std::string* args;
  vecu8* result;  // serialized protobuf message
};

struct ISArgs {
  yrclient::InstrumentationService* I;
  google::protobuf::Any* M;
};

/// Command to be executed in IService context
using IServiceCommand = std::function<std::unique_ptr<vecu8>(IServiceArgs)>;
template <typename T>
using aq_t = std::tuple<std::unique_lock<std::mutex>, T>;
using deleter_t = std::function<void(void*)>;
using storage_val = std::unique_ptr<void, deleter_t>;
using storage_t = std::map<std::string, storage_val>;

class InstrumentationService {
 public:
  InstrumentationService(const unsigned int max_clients,
                         const unsigned int port,
                         std::function<std::string(InstrumentationService*)>
                             on_shutdown = nullptr);
  ~InstrumentationService();
  void add_command_new(std::string name, command::Command::handler_t fn,
                       command::Command::deleter_t deleter);

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
  command::CommandManager& cmd_manager();
  server::Server& server();
  std::map<u8*, hook::Hook>& hooks();
  aq_t<std::map<u8*, hook::Hook>*> aq_hooks();
  aq_t<storage_t*> aq_storage();
  void store_value(const std::string key, vecu8* data);
  void store_value(const std::string key, void* data, deleter_t deleter);
  void* get_value(const std::string key, const bool acquire = true);
  void remove_value(const std::string key);
  // TODO: don't expose this
  std::function<std::string(InstrumentationService*)> on_shutdown_;

 private:
  yrclient::Response flush_results(
      const u64 queue_id, const std::chrono::milliseconds delay = 1000ms);
  yrclient::Response process_request(connection::Connection* C, vecu8* bytes);
  command::CommandManager cmd_manager_;
  server::Server server_;
  std::map<u8*, hook::Hook> hooks_;
  std::mutex mut_hooks_;
  storage_t storage_;
  std::mutex mut_storage_;
};

}  // namespace yrclient
