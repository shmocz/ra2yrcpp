#include "client_utils.hpp"
#include "config.hpp"
#include "debug_helpers.h"
#include "dll_inject.hpp"
#include "exprocess.hpp"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "is_context.hpp"
#include "utility/time.hpp"
#include "x86.hpp"

#include <xbyak/xbyak.h>

#include <libloaderapi.h>
#include <thread>
#include <vector>

using instrumentation_client::InstrumentationClient;

class DLLInjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    p_LoadLibrary = is_context::get_proc_address("LoadLibraryA");
    p_GetProcAddress = is_context::get_proc_address("GetProcAddress");
  }

  std::string path_dll{"build/src/libyrclient.dll"};
  std::string name_init{"init_iservice"};
  // FIXME: avoid hardcoding
  u32 p_LoadLibrary;
  u32 p_GetProcAddress;
  // void TearDown() override {}
};

struct B2STest : Xbyak::CodeGenerator {
  static void __cdecl copy_fn(char* dst, const char* src) {
    std::strcpy(dst, src);
  }
  B2STest(const std::string msg, void* dest) {
    vecu8 v1(msg.begin(), msg.end());
    v1.push_back(0x0);
    push(ebp);
    mov(ebp, esp);
    auto sz = x86::bytes_to_stack(this, v1);
    lea(eax, ptr[ebp - sz]);
    push(eax);
    push(reinterpret_cast<u32>(dest));
    mov(eax, reinterpret_cast<u32>(&copy_fn));
    call(eax);
    add(esp, sz + 0x8);
    pop(ebp);
    ret();
  }
};

TEST_F(DLLInjectTest, BytesToStackTest) {
  const std::string sm = "this is a test asd lol";
  std::string s = "";
  vecu8 dest(320, 0x0);
  for (size_t i = 1; i < sm.size(); i++) {
    dest.clear();
    std::string s = sm.substr(0, i);
    B2STest T(s, dest.data());
    auto p = T.getCode<void __cdecl (*)()>();
    p();
    std::string a(dest.begin(), dest.begin() + s.size());
    ASSERT_EQ(a, s);
  }
}

TEST_F(DLLInjectTest, BasicLoading) {
  auto g = LoadLibrary(path_dll.c_str());
  auto n = name_init.c_str();

  Xbyak::CodeGenerator C;
  is_context::get_procaddr(&C, g, name_init,
                           reinterpret_cast<u32>(&GetProcAddress));
  auto f = C.getCode<u32 __cdecl (*)()>();
  auto addr2 = f();

  auto addr1 = reinterpret_cast<u32>(GetProcAddress(g, n));
  ASSERT_NE(addr1, 0x0);
  ASSERT_EQ(addr1, addr2);
}

// TODO: strange error??? this fails if stderr is piped to a file
TEST_F(DLLInjectTest, IServiceDLLInjectTest) {
  is_context::DLLoader L(p_LoadLibrary, p_GetProcAddress, path_dll, name_init);
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  exprocess::ExProcess P("dummy_program.exe 10 500");

  util::sleep_ms(1000);
  dll_inject::suspend_inject_resume(P.handle(), sc);
  util::sleep_ms(1000);
  network::Init();
  auto client = InstrumentationClient("", std::to_string(cfg::SERVER_PORT));
  // run some commands
  std::string f1 = "flag1";
  std::string key = "key1";

  {
    yrclient::commands::StoreValue s;
    s.mutable_args()->set_key(key);
    s.mutable_args()->set_value(f1);
    auto r1 = client_utils::run(s, &client);
    // DPRINTF("RES %s\n", yrclient::commandsto_json(r1).c_str());

    yrclient::commands::GetValue g;
    g.mutable_args()->set_key(key);
    auto r2 = client_utils::run(g, &client);
    ASSERT_EQ(r2, f1);
  }

  auto s = client.shutdown();

  // NB. gotta wait explicitly, cuz WaitFoSingleObject could fail and we cant
  // throw from dtors
  P.join();
}

/// very basic test for the JIT code itself, mainly to catch major errors like
/// page faults. no cleanups done and injects to current process, so should be
/// skipped by default.
/// TODO: improve this to do proper unloadings or move to separate file
TEST_F(DLLInjectTest, DLLLoaderCodeTest) {
  GTEST_SKIP();
  is_context::DLLoader L(p_LoadLibrary, p_GetProcAddress, path_dll, name_init);
  auto p = L.getCode<void __cdecl (*)(void)>();
  p();
}
