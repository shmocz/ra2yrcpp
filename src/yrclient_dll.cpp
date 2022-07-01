#include "yrclient_dll.hpp"

#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "network.hpp"

#include <mutex>

using namespace yrclient_dll;

std::mutex g_lock;
yrclient::InstrumentationService* g_service = nullptr;
is_context::Context g_context;

yrclient::InstrumentationService* yrclient_dll::initialize(
    const unsigned int max_clients, const unsigned int port) {
  g_lock.lock();
  if (g_context.data() == nullptr) {
    network::Init();
    is_context::make_is_ctx(&g_context, max_clients, port);
  }
  g_lock.unlock();
}

void yrclient_dll::deinitialize() {
  g_lock.lock();
  if (g_service != nullptr) {
    delete g_service;
  }
  g_lock.unlock();
}

void init_iservice(const unsigned int max_clients, const unsigned int port) {
  yrclient_dll::initialize(max_clients, port);
}
