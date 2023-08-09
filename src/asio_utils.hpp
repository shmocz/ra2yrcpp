#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace ra2yrcpp {
namespace asio_utils {

class IOService {
 public:
  IOService();
  ~IOService();
  void post(std::function<void()> fn, const bool wait = true);
  void* get_service();

 private:
  struct IOService_impl;

  std::unique_ptr<IOService_impl> service_;
  std::unique_ptr<std::thread> main_thread_;
};

struct AsioSocket {
  AsioSocket();
  explicit AsioSocket(std::shared_ptr<IOService> srv);
  ~AsioSocket();

  void connect(const std::string host, const std::string port);
  /// Synchronously write the buffer to the socket
  ///
  /// @return amount of bytes transferred.
  /// @exception std::runtime_error on write failure
  std::size_t write(const std::string& buffer);

  /// Synchronously read from the socket.
  /// @exception std::runtime_error on read failure
  std::string read();

  std::shared_ptr<IOService> srv;
  class socket_impl;

  std::unique_ptr<socket_impl> socket_;
};

}  // namespace asio_utils
}  // namespace ra2yrcpp
