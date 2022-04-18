#pragma once
#include "protocol/protocol.hpp"
#include "types.h"
#include "server.hpp"
#include "command_manager.hpp"
#include "hook.hpp"
#include <vector>

namespace yrclient {

class InstrumentationService {
 public:
  InstrumentationService(const unsigned int max_clients,
                         const unsigned int port);
  ~InstrumentationService();
  void add_command();
  void remove_command();
  void install_hook_callback();
  void create_hook(const char* name, u8* target, const size_t code_length);
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

 private:
  yrclient::Response process_request(connection::Connection* C, vecu8* bytes);
  command_manager::CommandManager cmd_manager_;
  server::Server server_;
  std::vector<hook::Hook> hooks_;
};

};  // namespace yrclient
