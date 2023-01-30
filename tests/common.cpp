#include "common.hpp"

using namespace ra2yrcpp::tests;

void InstrumentationServiceTest::SetUp() {
  network::Init();
  yrclient::InstrumentationService::IServiceOptions O{cfg::MAX_CLIENTS,
                                                      cfg::SERVER_PORT, 0U, ""};

  I = std::unique_ptr<yrclient::InstrumentationService>(is_context::make_is(O));
  auto& S = I->server();
  client =
      std::make_unique<InstrumentationClient>(S.address(), S.port(), 5000ms);
  init();
}

void InstrumentationServiceTest::TearDown() {}
