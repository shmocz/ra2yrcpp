#include "common_multi.hpp"

#include <utility>

using namespace ra2yrcpp::tests;

MultiClientTestContext::MultiClientTestContext() {}

MultiClientTestContext::~MultiClientTestContext() { clients.clear(); }

void MultiClientTestContext::create_client(
    multi_client::AutoPollClient::Options o) {
  o.io_service = &srv;
  auto c = std::make_unique<multi_client::AutoPollClient>(o);
  c->start();
  clients.push_back(std::move(c));
}
