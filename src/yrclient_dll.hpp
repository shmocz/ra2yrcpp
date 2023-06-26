#pragma once

namespace yrclient_dll {
void initialize(const unsigned int max_clients, const unsigned int port,
                const unsigned ws_port, const bool no_init_hooks);
}  // namespace yrclient_dll

extern "C" {
__declspec(dllexport) void __cdecl init_iservice(const unsigned int max_clients,
                                                 unsigned int port,
                                                 unsigned int ws_port,
                                                 unsigned int no_init_hooks);
}
