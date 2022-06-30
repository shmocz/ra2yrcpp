#pragma once
#include "commands_yr.pb.h"
#include "protocol.pb.h"
#include "types.h"
#include "util_string.hpp"

#include <google/protobuf/util/json_util.h>
#include <string>

namespace yrclient {

// see issue #1
constexpr auto RESPONSE_OK = yrclient::ResponseCode::OK;
constexpr auto RESPONSE_ERROR = yrclient::ResponseCode::ERROR;

template <typename T>
inline vecu8 to_vecu8(const T& msg) {
  vecu8 res;
  res.resize(msg.ByteSizeLong());
  if (!msg.SerializeToArray(&res[0], res.size())) {
    res.resize(0);
  }
  return res;
}

template <typename T>
T from_any(const google::protobuf::Any& a) {
  T res;
  if (!a.UnpackTo(&res)) {
    throw std::runtime_error(std::string("Could not unpack message ") +
                             message_type(res));
  }
  return res;
}

std::string to_json(const google::protobuf::Message& m);
std::string message_type(const google::protobuf::Any& m);
std::string message_type(const google::protobuf::Message& m);
yrclient::Response make_response(const yrclient::ResponseCode code,
                                 const google::protobuf::Message& body);
}  // namespace yrclient
