#include "yrclient_dll.hpp"

#include "is_context.hpp"

using namespace yrclient_dll;

static std::mutex g_lock;
static yrclient::InstrumentationService* g_service = nullptr;
static is_context::Context g_context;

yrclient::InstrumentationService* yrclient_dll::initialize(
    const unsigned int max_clients, const unsigned int port) {
  g_lock.lock();
  if (g_context.data() == nullptr) {
    network::Init();
    is_context::make_is_ctx(&g_context, max_clients, port);
  }
  g_lock.unlock();
  return reinterpret_cast<yrclient::InstrumentationService*>(g_context.data());
}

// cppcheck-suppress unusedFunction
void yrclient_dll::deinitialize() {
  g_lock.lock();
  if (g_service != nullptr) {
    delete g_service;
  }
  g_lock.unlock();
}

// cppcheck-suppress unusedFunction
void init_iservice(const unsigned int max_clients, const unsigned int port) {
  yrclient_dll::initialize(max_clients, port);
}
