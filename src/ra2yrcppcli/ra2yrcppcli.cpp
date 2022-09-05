#include "ra2yrcppcli/ra2yrcppcli.hpp"

yrclient::Response ra2yrcppcli::send_command(multi_client::AutoPollClient* A,
                                             const std::string name,
                                             const std::string args) {
  yrclient::MessageBuilder B(name);
  auto* msg = yrclient::create_command_message(&B, args);
  return A->send_command(*msg);
}
