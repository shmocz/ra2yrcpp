#include "yrclient_dll.hpp"

#include "config.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"

#include <cstdlib>
#include <mutex>
#include <string>

static void* g_context = nullptr;

void ra2yrcpp::initialize(const unsigned int max_clients,
                          const unsigned int port, const bool no_init_hooks) {
  static std::mutex g_lock;
  g_lock.lock();
  auto* h = std::getenv("RA2YRCPP_ALLOWED_HOSTS_REGEX");
  if (g_context == nullptr) {
    ra2yrcpp::InstrumentationService::Options O{
        {cfg::SERVER_ADDRESS, port, max_clients,
         (h != nullptr ? h : cfg::ALLOWED_HOSTS_REGEX)},
        no_init_hooks};
    g_context = is_context::get_context(O);
  }

  g_lock.unlock();
}

// cppcheck-suppress unusedFunction
void init_iservice(const unsigned int max_clients, unsigned int port,
                   unsigned int no_init_hooks) {
  const auto* tcp_port = std::getenv("RA2YRCPP_PORT");

  ra2yrcpp::initialize(max_clients,
                       (tcp_port != nullptr) ? std::stol(tcp_port) : port,
                       no_init_hooks > 0U);
}
