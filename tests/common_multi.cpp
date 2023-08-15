#include "common_multi.hpp"

#include "asio_utils.hpp"

#include <utility>

using namespace ra2yrcpp::tests;

MultiClientTestContext::MultiClientTestContext()
    : srv(std::make_shared<asio_utils::IOService>()) {}

MultiClientTestContext::~MultiClientTestContext() { clients.clear(); }

void MultiClientTestContext::create_client(const AutoPollClient::Options o) {
  auto c = std::make_unique<multi_client::AutoPollClient>(srv, o);
  c->start();
  clients.push_back(std::move(c));
}
