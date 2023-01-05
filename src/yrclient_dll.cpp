#include "yrclient_dll.hpp"

#include "is_context.hpp"

using namespace yrclient_dll;

static yrclient::InstrumentationService* g_service = nullptr;

static auto get_context() {
  static auto ctx = std::make_unique<is_context::Context>();
  return ctx.get();
}

yrclient::InstrumentationService* yrclient_dll::initialize(
    const unsigned int max_clients, const unsigned int port) {
  static std::mutex g_lock;
  g_lock.lock();
  if (get_context()->data() == nullptr) {
    network::Init();
    is_context::make_is_ctx(get_context(), max_clients, port);
  }
  g_lock.unlock();
  return reinterpret_cast<yrclient::InstrumentationService*>(
      get_context()->data());
}

// cppcheck-suppress unusedFunction
void yrclient_dll::deinitialize() {
  if (g_service != nullptr) {
    delete g_service;
  }
}

// cppcheck-suppress unusedFunction
void init_iservice(const unsigned int max_clients, unsigned int port) {
  if (port == 0U) {
    port = std::stol(std::getenv("RA2YRCPP_PORT"));
  }
  (void)yrclient_dll::initialize(max_clients, port);
}
