#pragma once

namespace ra2yrcpp {
void initialize(unsigned int max_clients, unsigned int port,
                bool no_init_hooks);
}  // namespace ra2yrcpp

extern "C" {
__declspec(dllexport) void __cdecl init_iservice(unsigned int max_clients,
                                                 unsigned int port,
                                                 unsigned int no_init_hooks);
}
