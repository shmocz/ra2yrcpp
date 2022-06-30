#include "protocol.hpp"

std::string yrclient::to_json(const google::protobuf::Message& m) {
  std::string res;
  google::protobuf::util::MessageToJsonString(m, &res);
  return res;
}

#if 1
std::string yrclient::message_type(const google::protobuf::Any& m) {
  auto toks = yrclient::split_string(m.type_url(), "/");
  return toks.back();
}

std::string yrclient::message_type(const google::protobuf::Message& m) {
  return m.GetTypeName();
}
#endif

yrclient::Response yrclient::make_response(
    const yrclient::ResponseCode code, const google::protobuf::Message& body) {
  yrclient::Response r;
  r.set_code(code);
  if (!r.mutable_body()->PackFrom(body)) {
    throw std::runtime_error("Could not pack message body");
  }
  return r;
}
