#include "yrclient_dll.hpp"

#include "instrumentation_service.hpp"
#include "is_context.hpp"

#include <memory>
#include <mutex>

static void* g_context = nullptr;

void yrclient_dll::initialize(const unsigned int max_clients,
                              const unsigned int port,
                              const bool no_init_hooks) {
  static std::mutex g_lock;
  g_lock.lock();
  if (g_context == nullptr) {
    yrclient::InstrumentationService::Options O{
        {cfg::SERVER_ADDRESS, port, max_clients}, no_init_hooks};
    g_context = is_context::get_context(O);
  }

  g_lock.unlock();
}

// cppcheck-suppress unusedFunction
void init_iservice(const unsigned int max_clients, unsigned int port,
                   unsigned int no_init_hooks) {
  auto* tcp_port = std::getenv("RA2YRCPP_PORT");

  yrclient_dll::initialize(max_clients,
                           (tcp_port != nullptr) ? std::stol(tcp_port) : port,
                           no_init_hooks > 0U);
}
