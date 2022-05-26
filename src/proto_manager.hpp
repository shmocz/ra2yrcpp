#pragma once
#include "protocol/protocol.hpp"
#include <string>
#include <map>
#include <functional>

namespace proto_manager {
using handler_t = std::function<google::protobuf::Message*()>;
class ProtoManager {
 public:
  ProtoManager();
  void add_handler(const std::string type_url, handler_t handler);
  google::protobuf::Message* get_message(const std::string type_url);

 private:
  std::map<std::string, handler_t> handlers_;
};
}  // namespace proto_manager
