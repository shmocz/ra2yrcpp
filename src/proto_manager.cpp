#include "proto_manager.hpp"
#include "debug_helpers.h"

using namespace proto_manager;

ProtoManager::ProtoManager() {}

void ProtoManager::add_handler(const std::string type_url, handler_t handler) {
  DPRINTF("type_url=%s\n", type_url.c_str());
  handlers_[type_url] = handler;
}
google::protobuf::Message* ProtoManager::get_message(
    const std::string type_url) {
  DPRINTF("type_url=%s\n", type_url.c_str());
  return handlers_.at(type_url)();
}
