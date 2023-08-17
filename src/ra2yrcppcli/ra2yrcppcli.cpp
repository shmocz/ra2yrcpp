#include "ra2yrcppcli/ra2yrcppcli.hpp"

#include "multi_client.hpp"
#include "protocol/helpers.hpp"

ra2yrproto::Response ra2yrcppcli::send_command(multi_client::AutoPollClient* A,
                                               const std::string name,
                                               const std::string args) {
  ra2yrcpp::protocol::MessageBuilder B(name);
  auto* msg = ra2yrcpp::protocol::create_command_message(&B, args);
  return A->send_command(*msg);
}
