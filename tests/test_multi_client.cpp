#include "protocol/protocol.hpp"

#include "client_utils.hpp"
#include "commands_builtin.hpp"
#include "common_multi.hpp"
#include "config.hpp"
#include "connection.hpp"
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "multi_client.hpp"
#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/core.pb.h"
#include "types.h"
#include "util_string.hpp"
#include "utility/time.hpp"
#include "websocket_server.hpp"

#include <google/protobuf/io/gzip_stream.h>

#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <regex>
#include <thread>
#include <vector>

using namespace yrclient;
using namespace std::chrono_literals;
using instrumentation_client::InstrumentationClient;
using namespace multi_client;

using ra2yrcpp::tests::MultiClientTestContext;

class MultiClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network::Init();
    yrclient::InstrumentationService::IServiceOptions opts{
        cfg::MAX_CLIENTS, cfg::SERVER_PORT, cfg::WEBSOCKET_PROXY_PORT,
        cfg::SERVER_ADDRESS, true};
    AutoPollClient::Options aopts{
        opts.host, std::to_string(opts.ws_port), 1.0s,
        0.25s,     CONNECTION_TYPE::WEBSOCKET,   nullptr};

    std::map<std::string, command::Command::handler_t> cmds;

    for (auto& [name, fn] : yrclient::commands_builtin::get_commands()) {
      cmds[name] = fn;
    }

    I = std::unique_ptr<yrclient::InstrumentationService>(
        yrclient::InstrumentationService::create(opts, &cmds, nullptr));
    ctx = std::make_unique<MultiClientTestContext>();
    ctx->create_client(aopts);
  }

  void TearDown() override {
    ctx = nullptr;
    I = nullptr;
    network::Deinit();
  }

  // FIXME: need to ensure websocketproxy is properly setup before starting
  // conns
  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<MultiClientTestContext> ctx;

  template <typename T>
  auto run_async(const T& cmd) {
    auto r = ctx->clients[0]->send_command(cmd);
    auto cmd_res = yrclient::from_any<ra2yrproto::CommandResult>(r.body());
    return yrclient::from_any<T>(cmd_res.result()).result();
  }
};

TEST_F(MultiClientTest, RunRegularCommand) {
  const unsigned count = 5u;

  ra2yrproto::commands::GetSystemState cmd;
  for (auto i = 0u; i < count; i++) {
    auto r = run_async<decltype(cmd)>(cmd);
    ASSERT_EQ(r.state().connections().size(), 2);
  }
}

TEST_F(MultiClientTest, RunCommandsAndVerify) {
  const int count = 10;
  const int val_size = 128;
  const std::string key = "tdata";
  for (int i = 0; i < count; i++) {
    std::string k1(val_size, static_cast<char>(i));
    ra2yrproto::commands::StoreValue sv;
    sv.mutable_args()->set_value(k1);
    sv.mutable_args()->set_key(key);
    auto r1 = run_async<decltype(sv)>(sv);

    ra2yrproto::commands::GetValue gv;
    gv.mutable_args()->set_key(key);
    auto r2 = run_async<decltype(gv)>(gv);
    ASSERT_EQ(r2.result(), k1);
  }
}

namespace lib = websocketpp::lib;

struct AsioSocket {
  explicit AsioSocket(lib::asio::ip::tcp::socket s) : s(std::move(s)) {}

  ~AsioSocket() {
    s.shutdown(lib::asio::socket_base::shutdown_both);
    s.close();
  }

  websocketpp::lib::asio::ip::tcp::socket s;
};

TEST_F(MultiClientTest, TestHTTPRequest) {
  // FIXME(shmocz): this is required or otherwise plain JSON won't work
  // and fail with error:
  // /home/user/project/src/multi_client.cpp:poll_thread:129 fatal error: Could
  // not unpack message ra2yrproto.PollResults yrclient::MessageBuilder

#if 0
  B("ra2yrproto.commands.GetSystemState");
  Use these commands to re - generate the string message.auto* mm =
      yrclient::create_command_message(&B, "");
  auto cc =
      yrclient::create_command(*mm, ra2yrproto::CommandType::CLIENT_COMMAND);
  cc.set_blocking(true);
  auto cmd_json = yrclient::to_json(cc);
#endif
  // FIXME(shmocz): adding this delay fixes the unpack error. Race condition?
  std::string cmd_json2 =
      "{\"commandType\":\"CLIENT_COMMAND\",\"command\":{\"@type\":\"type."
      "googleapis.com/"
      "ra2yrproto.commands.GetSystemState\"},\"blocking\":true}";
  // dprintf("json={}", cmd_json);
  // dprintf("json={}", cmd_json2);

  std::string msg = fmt::format(
      "POST / HTTP/1.1\r\nHost: 127.0.0.1:14525\r\nAccept: "
      "*/*\r\nContent-Type: application/json\r\nContent-Length: "
      "{}\r\n\r\n{}",
      cmd_json2.size(), cmd_json2);

  // ra2yrcpp::asio_utils::IOService S;
  auto& S = ctx->srv;

  for (int i = 0; i < 32; i++) {
    AsioSocket A(websocketpp::lib::asio::ip::tcp::socket(
        *reinterpret_cast<websocketpp::lib::asio::io_service*>(
            S.service_.get())));
    auto& sock = A.s;
    sock.connect(lib::asio::ip::tcp::endpoint{
        lib::asio::ip::address_v4::from_string(I->opts().host),
        static_cast<u16>(I->opts().ws_port)});
    lib::asio::write(sock, lib::asio::buffer(msg));

    std::string rsp;
    lib::asio::error_code ec;

    // Read the whole message
    lib::asio::read(sock, lib::asio::dynamic_buffer(rsp),
                    lib::asio::transfer_all(), ec);
    if (!(!ec || ec == lib::asio::error::eof)) {
      throw std::runtime_error("failed to read");
    }

    // Get content-length
    std::regex re("Content-Length: (\\d+)");
    std::smatch match;

    if (std::regex_search(rsp, match, re) && match.size() > 1) {
      auto bbody = yrclient::to_bytes(rsp.substr(rsp.find("\r\n\r\n") + 4));
      ra2yrproto::Response R;
      ASSERT_TRUE(yrclient::from_json(bbody, &R));
    }
  }

  // Get actual data
}
