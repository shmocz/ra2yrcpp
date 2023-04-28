#include "common_multi.hpp"

using namespace ra2yrcpp::tests;

MultiClientTestContext::MultiClientTestContext() {}

MultiClientTestContext::~MultiClientTestContext() { clients.clear(); }

void MultiClientTestContext::create_client(
    multi_client::AutoPollClient::Options o) {
  o.ctype = multi_client::CONNECTION_TYPE::WEBSOCKET;
  o.io_service = srv.service_.get();
  clients.push_back(std::make_unique<multi_client::AutoPollClient>(o));
}
