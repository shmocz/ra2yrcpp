#pragma once
#include "instrumentation_service.hpp"
#include "is_context.hpp"

namespace yrclient_dll {
yrclient::InstrumentationService* initialize(const unsigned int max_clients,
                                             const unsigned int port);
void deinitialize();
}  // namespace yrclient_dll

extern "C" void __cdecl init_iservice(const unsigned int max_clients,
                                      const unsigned int port);
