#include "ra2yrproto/commands_builtin.pb.h"

#include "asio_utils.hpp"
#include "client_connection.hpp"
#include "client_utils.hpp"
#include "config.hpp"
#include "dll_inject.hpp"
#include "instrumentation_client.hpp"
#include "is_context.hpp"
#include "logging.hpp"
#include "types.h"
#include "util_proto.hpp"
#include "utility/time.hpp"
#include "websocket_connection.hpp"
#include "win32/windows_utils.hpp"
#include "x86.hpp"

#include <gtest/gtest.h>
#include <xbyak/xbyak.h>

#include <cstring>

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using instrumentation_client::InstrumentationClient;
using namespace std::chrono_literals;
using namespace ra2yrcpp::test_util;
using namespace ra2yrcpp;

class DLLInjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    o.PA.p_LoadLibrary = windows_utils::get_proc_address("LoadLibraryA");
    o.PA.p_GetProcAddress = windows_utils::get_proc_address("GetProcAddress");
  }

  is_context::DLLLoader::Options o{is_context::default_options};
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
  for (size_t i = 1; i < sm.size(); i++) {
    vecu8 dest(320, 0x0);
    std::string s = sm.substr(0, i);
    B2STest T(s, dest.data());
    auto p = T.getCode<void __cdecl (*)()>();
    p();
    std::string a(dest.begin(), dest.begin() + s.size());
    ASSERT_EQ(a, s);
  }
}

TEST_F(DLLInjectTest, BasicLoading) {
  auto addrs = is_context::get_procaddrs();
  auto g = windows_utils::load_library(o.path_dll.c_str());

  Xbyak::CodeGenerator C;
  is_context::get_procaddr(&C, g, o.name_init, addrs.p_GetProcAddress);
  auto f = C.getCode<u32 __cdecl (*)()>();
  auto addr2 = f();
  auto addr1 = windows_utils::get_proc_address(o.name_init, g);

  ASSERT_NE(addr1, 0x0);
  ASSERT_EQ(addr1, addr2);
}

TEST_F(DLLInjectTest, IServiceDLLInjectTest) {
  auto opts = o;
  opts.no_init_hooks = true;
  is_context::DLLLoader L(opts);
  L.ret();
  auto p = L.getCode<u8*>();
  vecu8 sc(p, p + L.getSize());
  windows_utils::ExProcess P("dummy_program.exe 10 500");

  dll_inject::suspend_inject_resume(P.handle(), sc,
                                    dll_inject::DLLInjectOptions());

  std::unique_ptr<InstrumentationClient> client;
  asio_utils::IOService srv;

  util::call_until(5.0s, 1.0s, [&]() {
    try {
      auto conn = std::make_shared<connection::ClientWebsocketConnection>(
          cfg::SERVER_ADDRESS, std::to_string(o.port), &srv);
      conn->connect();
      client = std::make_unique<InstrumentationClient>(conn);
      return false;
    } catch (const std::exception& e) {
      eprintf("fail: {}", e.what());
      return true;
    }
  });

  ASSERT_NE(client.get(), nullptr);

  // run some commands
  {
    std::string f1 = "flag1";
    std::string key = "key1";

    // TODO(shmocz): don't ignore return value
    (void)client_utils::run(StoreValue::create({key, f1}), client.get());
    auto r2 = client_utils::run(GetValue::create({key, ""}), client.get());
    ASSERT_EQ(r2.value(), f1);
  }

  client = nullptr;

  // NB. gotta wait explicitly, cuz WaitFoSingleObject could fail and we cant
  // throw from dtors
  P.join();
}
