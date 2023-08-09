#include "asio_utils.hpp"

#include "logging.hpp"
#include "types.h"
#include "utility/sync.hpp"

#include <asio/executor_work_guard.hpp>
#include <fmt/core.h>
#include <websocketpp/config/asio_no_tls.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>

using namespace ra2yrcpp::asio_utils;
namespace lib = websocketpp::lib;
using ios_t = lib::asio::io_context;

class AsioSocket::socket_impl : public lib::asio::ip::tcp::socket {
 public:
  explicit socket_impl(ios_t& io) : lib::asio::ip::tcp::socket(io) {}  // NOLINT
};

/// Submit function to be executed by io_service
static void do_post(void* ptr, std::function<void()> fn) {
  lib::asio::post(*reinterpret_cast<ios_t*>(ptr), fn);
}

/// Submit a function to be executed by io_service and wait for it's completion.
static void post_(void* ios, std::function<void()> fn) {
  util::AtomicVariable<bool> done(false);
  do_post(ios, [fn, &done]() {
    fn();
    done.store(true);
  });

  done.wait(true);
}

struct IOService::IOService_impl {
  ios_t service_;
  lib::asio::executor_work_guard<ios_t::executor_type> guard_;

  IOService_impl() : guard_(lib::asio::make_work_guard(service_)) {}

  void run() { service_.run(); }

  void reset() { guard_.reset(); }
};

IOService::IOService()
    : service_(std::make_unique<IOService_impl>()), main_thread_() {
  main_thread_ = std::make_unique<std::thread>(
      [](IOService_impl* srv) { srv->run(); }, service_.get());
}

IOService::~IOService() {
  service_->reset();
  main_thread_->join();
  dprintf("exit main thread");
}

void IOService::post(std::function<void()> fn, const bool wait) {
  auto* s = &service_->service_;
  if (wait) {
    post_(s, fn);
  } else {
    do_post(s, fn);
  }
}

void* IOService::get_service() {
  return reinterpret_cast<void*>(&service_->service_);
}

AsioSocket::AsioSocket() {}

AsioSocket::AsioSocket(std::shared_ptr<IOService> srv)
    : srv(srv),
      socket_(std::make_unique<AsioSocket::socket_impl>(
          *reinterpret_cast<ios_t*>(srv->get_service()))) {}

AsioSocket::~AsioSocket() {
  lib::error_code ec;
  auto& s = *socket_.get();

  s.shutdown(lib::asio::socket_base::shutdown_both, ec);
  if (ec) {
    eprintf("socket shutdown failed: ", ec.message());
  }
  s.close(ec);
  if (ec) {
    eprintf("socket close failed: ", ec.message());
  }
}

void AsioSocket::connect(const std::string host, const std::string port) {
  socket_->connect(
      lib::asio::ip::tcp::endpoint{lib::asio::ip::address_v4::from_string(host),
                                   static_cast<u16>(std::stoi(port))});
}

std::size_t AsioSocket::write(const std::string& buffer) {
  lib::error_code ec;
  std::size_t count =
      lib::asio::write(*socket_.get(), lib::asio::buffer(buffer));

  if (ec) {
    throw std::runtime_error(fmt::format("failed to write: {}", ec.message()));
  }
  return count;
}

std::string AsioSocket::read() {
  lib::error_code ec;
  std::string rsp;
  lib::asio::read(*socket_.get(), lib::asio::dynamic_buffer(rsp),
                  lib::asio::transfer_all(), ec);
  if (!(!ec || ec == lib::asio::error::eof)) {
    throw std::runtime_error(fmt::format("failed to write: {}", ec.message()));
  }
  return rsp;
}
