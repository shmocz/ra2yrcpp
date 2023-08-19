#include "common_multi.hpp"

#include "asio_utils.hpp"

using namespace ra2yrcpp::tests;

MultiClientTestContext::MultiClientTestContext()
    : srv(std::make_shared<asio_utils::IOService>()) {}

MultiClientTestContext::~MultiClientTestContext() { clients.clear(); }

void MultiClientTestContext::create_client(const AutoPollClient::Options o) {
  clients.push_back(std::make_unique<multi_client::AutoPollClient>(srv, o));
  clients.back()->start();
}
