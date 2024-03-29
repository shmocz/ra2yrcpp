#pragma once

namespace ra2yrcpp {
void initialize(const unsigned int max_clients, const unsigned int port,
                const bool no_init_hooks);
}  // namespace ra2yrcpp

extern "C" {
__declspec(dllexport) void __cdecl init_iservice(const unsigned int max_clients,
                                                 unsigned int port,
                                                 unsigned int no_init_hooks);
}
