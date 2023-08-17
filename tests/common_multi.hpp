#pragma once
#include "multi_client.hpp"

#include <memory>
#include <vector>

namespace ra2yrcpp {
namespace asio_utils {
class IOService;
}
}  // namespace ra2yrcpp

namespace ra2yrcpp {
namespace tests {

namespace {
using multi_client::AutoPollClient;
}  // namespace

struct MultiClientTestContext {
  std::shared_ptr<ra2yrcpp::asio_utils::IOService> srv;
  std::vector<std::unique_ptr<AutoPollClient>> clients;

  MultiClientTestContext();

  ~MultiClientTestContext();

  void create_client(const AutoPollClient::Options o);
};
}  // namespace tests
}  // namespace ra2yrcpp
