#pragma once

namespace ra2yrcpp {

/// This function should only be called if the library is to be initialized
/// DLLLoader patch instead of Syringe, such in the case of legacy CnCNet
/// spawner.
void initialize(unsigned int max_clients, unsigned int port,
                bool no_init_hooks);
}  // namespace ra2yrcpp

extern "C" {
__declspec(dllexport) void __cdecl init_iservice(unsigned int max_clients,
                                                 unsigned int port,
                                                 unsigned int no_init_hooks);
}
