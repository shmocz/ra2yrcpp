#pragma once
#include "gtest/gtest.h"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"

#include <cstdio>

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

namespace ra2yrcpp {

using instrumentation_client::InstrumentationClient;

namespace {
using namespace std::chrono_literals;
}  // namespace

namespace tests {

class InstrumentationServiceTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  virtual void init() = 0;
  std::unique_ptr<yrclient::InstrumentationService> I;
  std::unique_ptr<InstrumentationClient> client;
};
}  // namespace tests
}  // namespace ra2yrcpp
