#pragma once
#include "async_queue.hpp"
#include "client_connection.hpp"
#include "types.h"

#include <memory>
#include <string>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

namespace ra2yrcpp::connection {
class ClientWebsocketConnection : public ClientConnection {
 public:
  ClientWebsocketConnection(std::string host, std::string port,
                            ra2yrcpp::asio_utils::IOService* io_service);
  using item_t = std::shared_ptr<vecu8>;
  ~ClientWebsocketConnection() override;
  void connect() override;
  void send_data(const vecu8& bytes) override;
  vecu8 read_data() override;
  /// Puts empty vector to input message queue to signal the reader that
  /// connection is closed.
  void stop() override;
  class client_impl;

 private:
  async_queue::AsyncQueue<item_t> in_q_;
  std::unique_ptr<client_impl> client_;
  ra2yrcpp::asio_utils::IOService* io_service_;
  std::weak_ptr<void> connection_handle_;
};
}  // namespace ra2yrcpp::connection
