#include "yrclient_dll.hpp"

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
  if (port == 0U) {
    port = std::stol(std::getenv("RA2YRCPP_PORT"));
  }
  if (ws_port == 0U) {
    auto* p = std::getenv("RA2YRCPP_WS_PORT");
    ws_port = (p != nullptr) ? std::stol(p) : 0U;
  }

  yrclient_dll::initialize(max_clients, port, ws_port, no_init_hooks > 0U);
}
