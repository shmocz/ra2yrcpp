#include "is_context.hpp"

#include "x86.hpp"

using namespace is_context;
using x86::bytes_to_stack;

u32 is_context::get_proc_address(const std::string addr) {
  return reinterpret_cast<u32>(
      GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), addr.c_str()));
}

// TODO: utilize in tests
ProcAddrs is_context::get_procaddrs() {
  ProcAddrs A;
  A.p_LoadLibrary = get_proc_address("LoadLibraryA");
  A.p_GetProcAddress = get_proc_address("GetProcAddress");
  return A;
}

vecu8 is_context::vecu8cstr(const std::string s) {
  auto c = s.c_str();
  vecu8 r(c, c + s.size() + 1);
  return r;
}

void is_context::make_is_ctx(Context* c, const unsigned int max_clients,
                             const unsigned int port) {
  auto* I = new yrclient::InstrumentationService(
      max_clients, port, [c](auto* X) { return c->on_signal(); });
  c->data() = reinterpret_cast<void*>(I);
  c->deleter() = [](Context* ctx) {
    delete reinterpret_cast<decltype(I)>(ctx->data());
  };
  c->set_on_signal([](Context* ctx) {
    auto* II = reinterpret_cast<decltype(I)>(ctx->data());
    return II->on_shutdown_(II);
  });
}

DLLoader::DLLoader(u32 p_LoadLibrary, u32 p_GetProcAddress,
                   const std::string path_dll, const std::string name_init,
                   const unsigned int max_clients, const unsigned int port) {
  vecu8 v1(path_dll.begin(), path_dll.end());
  v1.push_back(0x0);
  vecu8 v2(name_init.begin(), name_init.end());
  v2.push_back(0x0);
  x86::save_regs(this);
  // Call LoadLibrary
  push(ebp);
  mov(ebp, esp);
  auto sz = bytes_to_stack(this, v1);
  lea(eax, ptr[ebp - sz]);
  push(eax);
  mov(eax, p_LoadLibrary);
  call(eax);  // TODO: handle errors
  // restore stack
  mov(esp, ebp);
  push(eax);                      // save init fn address
  sz = bytes_to_stack(this, v2);  // lpProcName
  lea(eax, ptr[ebp - sz - 0x4]);
  push(eax);  // &lpProcName
  mov(eax, ptr[ebp - 0x4]);
  push(eax);  // hModule
  mov(eax, p_GetProcAddress);
  call(eax);  // GetProcAddress(hModule, lpProcName)
  // Call init routine
  push(port);
  push(max_clients);
  call(eax);
  // Restore registers
  mov(esp, ebp);
  pop(ebp);
  x86::restore_regs(this);
  ret();
}

void is_context::get_procaddr(Xbyak::CodeGenerator* c, HMODULE m,
                              const std::string name,
                              const u32 p_GetProcAddress) {
  using namespace Xbyak::util;
  vecu8 n = vecu8cstr(name);
  c->push(ebp);
  c->mov(ebp, esp);
  auto sz = bytes_to_stack(c, n);
  c->lea(eax, ptr[ebp - sz]);
  c->push(eax);
  c->push(reinterpret_cast<u32>(m));
  c->mov(eax, p_GetProcAddress);
  c->call(eax);
  // restore stack
  c->mov(esp, ebp);
  c->pop(ebp);
  c->ret();
}
