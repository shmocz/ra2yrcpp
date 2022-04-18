#include "protocol.hpp"
#include "google/protobuf/util/json_util.h"

std::string yrclient::to_json(const google::protobuf::Message& m) {
  std::string res;
  google::protobuf::util::MessageToJsonString(m, &res);
  return res;
}
