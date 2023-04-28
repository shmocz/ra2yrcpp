#include "asio_utils.hpp"

#include <websocketpp/config/asio_no_tls.hpp>

using namespace ra2yrcpp::asio_utils;
using ios_t = websocketpp::lib::asio::io_service;
using work_guard_t =
    websocketpp::lib::asio::executor_work_guard<ios_t::executor_type>;

template <typename T>
static auto vget(const std::unique_ptr<void, void (*)(void*)>& ptr) {
  return reinterpret_cast<T*>(ptr.get());
}

IOService::IOService()
    : service_(utility::make_uptr<ios_t>()),
      work_guard_(utility::make_uptr<work_guard_t>(
          asio::make_work_guard(*vget<ios_t>(service_)))),
      main_thread_([this]() { vget<ios_t>(service_)->run(); }) {}

IOService::~IOService() {
  vget<work_guard_t>(work_guard_)->reset();
  main_thread_.join();
}

void IOService::post(std::function<void()> fn) {
  vget<ios_t>(service_)->post(fn);
}
