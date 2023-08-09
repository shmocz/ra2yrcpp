#pragma once
#include "asio_utils.hpp"
#include "multi_client.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace ra2yrcpp {
namespace tests {

namespace {
using namespace std::chrono_literals;
using multi_client::AutoPollClient;
}  // namespace

struct MultiClientTestContext {
  ra2yrcpp::asio_utils::IOService srv;
  std::vector<std::unique_ptr<AutoPollClient>> clients;

  MultiClientTestContext();

  ~MultiClientTestContext();

  void create_client(multi_client::AutoPollClient::Options o);
};
}  // namespace tests
}  // namespace ra2yrcpp
