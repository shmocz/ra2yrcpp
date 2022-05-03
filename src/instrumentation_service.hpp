#pragma once
#include "protocol/protocol.hpp"
#include "types.h"
#include "server.hpp"
#include "command_manager.hpp"
#include "hook.hpp"
#include <mutex>
#include <map>
#include <memory>
#include <tuple>
#include <string>
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

using IServiceCommand =
    std::function<command_manager::CommandResult(IServiceArgs)>;

class InstrumentationService {
 public:
  InstrumentationService(const unsigned int max_clients,
                         const unsigned int port);
  ~InstrumentationService();
  void add_command(std::string name, IServiceCommand cmd);
  void remove_command();
  void install_hook_callback();
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
  void store_value(const std::string key, std::unique_ptr<vecu8> data);
  vecu8* get_value(const std::string key);
  void remove_value(const std::string key);

 private:
  void add_builtin_commands();
  yrclient::Response flush_results(const size_t id);
  yrclient::Response process_request(connection::Connection* C, vecu8* bytes);
  command_manager::CommandManager cmd_manager_;
  server::Server server_;
  std::map<u8*, hook::Hook> hooks_;
  std::map<std::string, std::unique_ptr<vecu8>> storage_;
};

};  // namespace yrclient
