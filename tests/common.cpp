#include "common.hpp"

using namespace ra2yrcpp::tests;

void InstrumentationServiceTest::SetUp() {
  network::Init();
  I = std::unique_ptr<yrclient::InstrumentationService>(
      is_context::make_is(cfg::MAX_CLIENTS, cfg::SERVER_PORT));
  auto& S = I->server();
  client = std::make_unique<InstrumentationClient>(S.address(), S.port(),
                                                   5000ms, 10ms);
  init();
}

void InstrumentationServiceTest::TearDown() {}
