#pragma once
#include "types.h"
#include "protocol.pb.h"
#include "commands_yr.pb.h"
#include <google/protobuf/util/json_util.h>
#include "debug_helpers.h"
#include "util_string.hpp"
#include <string>
#include <regex>

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

std::string to_json(const google::protobuf::Message& m);
std::string message_type(const google::protobuf::Any& m);
std::string message_type(const google::protobuf::Message& m);

}  // namespace yrclient
