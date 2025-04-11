#include "yrclient_dll.hpp"

#include "constants.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "ra2yrcpp.hpp"

#include <cstdio>
#include <cstdlib>

#include <mutex>
#include <string>

void ra2yrcpp::initialize(unsigned int max_clients, unsigned int port,
                          bool no_init_hooks) {
  (void)max_clients;
  (void)port;
  static std::mutex g_lock;
  g_lock.lock();
  auto* I = ra2yrcpp::Main::get();
  if (no_init_hooks) {
    I->load_configuration(nullptr);
    I->start_service();
  } else {
    I->create_all_hooks();
  }

  g_lock.unlock();
}

// cppcheck-suppress unusedFunction
void init_iservice(unsigned int max_clients, unsigned int port,
                   unsigned int no_init_hooks) {
  ra2yrcpp::initialize(max_clients, port, no_init_hooks > 0U);
}

// cppcheck-suppress unusedFunction
int __stdcall DllMain(HANDLE hInstance, DWORD dwReason, LPVOID v) {
  (void)hInstance;
  (void)v;
  if (dwReason == DLL_PROCESS_ATTACH) {
    (void)ra2yrcpp::logging::set_output_handle(cfg::LOG_FILE_NAME);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    ra2yrcpp::logging::close_output_handle();
  }
  return 1;
}
