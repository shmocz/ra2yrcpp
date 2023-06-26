#include "yrclient_dll.hpp"

#include "is_context.hpp"
#include "network.hpp"

#include <memory>
#include <mutex>

static void* g_context = nullptr;

void yrclient_dll::initialize(const unsigned int max_clients,
                              const unsigned int port, const unsigned ws_port,
                              const bool no_init_hooks) {
  static std::mutex g_lock;
  g_lock.lock();
  if (g_context == nullptr) {
    g_context =
        is_context::get_context(max_clients, port, ws_port, no_init_hooks);
  }
  g_lock.unlock();
}

// FIXME: ws_port unused?
// cppcheck-suppress unusedFunction
void init_iservice(const unsigned int max_clients, unsigned int port,
                   unsigned int ws_port, unsigned int no_init_hooks) {
  auto* tcp_port = std::getenv("RA2YRCPP_PORT");
  auto* p = std::getenv("RA2YRCPP_WS_PORT");

  yrclient_dll::initialize(
      max_clients, (tcp_port != nullptr) ? std::stol(tcp_port) : port,
      (p != nullptr) ? std::stol(p) : ws_port, no_init_hooks > 0U);
}
