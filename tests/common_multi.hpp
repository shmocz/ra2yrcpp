#pragma once
#include "connection.hpp"
#include "instrumentation_client.hpp"
#include "instrumentation_service.hpp"
#include "multi_client.hpp"
#include "utility/memtools.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

namespace ra2yrcpp {
namespace tests {

namespace {
using namespace std::chrono_literals;
using multi_client::AutoPollClient;
}  // namespace

struct MultiClientTestContext {
  std::unique_ptr<void, void (*)(void*)> io_service_guard;
  std::future<void> fut_io_service;
  std::vector<std::unique_ptr<AutoPollClient>> clients;

  MultiClientTestContext();

  ~MultiClientTestContext();

  void create_client(multi_client::AutoPollClient::Options o);
};
}  // namespace tests
}  // namespace ra2yrcpp
