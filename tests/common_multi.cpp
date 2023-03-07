#include "common_multi.hpp"

using namespace ra2yrcpp::tests;

using io_service_t = ra2yrcpp::websocket_server::IOService;

MultiClientTestContext::MultiClientTestContext()
    : io_service_guard(utility::make_uptr<io_service_t>()) {}

MultiClientTestContext::~MultiClientTestContext() {
  clients.clear();
  // FIME: may not be needed
  io_service_guard = nullptr;
}

void MultiClientTestContext::create_client(
    multi_client::AutoPollClient::Options o) {
  o.ctype = multi_client::CONNECTION_TYPE::WEBSOCKET;
  o.io_service = &(reinterpret_cast<io_service_t*>(io_service_guard.get())->s);
  clients.push_back(std::make_unique<multi_client::AutoPollClient>(o));
}
