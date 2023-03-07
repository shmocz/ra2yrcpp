#pragma once
#include "connection.hpp"
#include "types.h"
#include "utility/sync.hpp"

#include <memory>
#include <string>
#include <vector>

namespace connection {
class ClientWebsocketConnection : public ClientConnection {
 public:
  ClientWebsocketConnection(std::string host, std::string port,
                            void* io_service);
  using item_t = std::shared_ptr<vecu8>;
  ~ClientWebsocketConnection() override;
  void connect() override;
  bool send_data(const std::vector<u8>& bytes) override;
  vecu8 read_data() override;
  //
  // Puts empty vector to input message queue to signal the reader that
  // connection is closed.
  //
  void stop() override;

 private:
  async_queue::AsyncQueue<item_t> in_q_;
  std::unique_ptr<void, void (*)(void*)> client_;
  void* io_service_;
  std::weak_ptr<void> connection_handle_;
};
}  // namespace connection
