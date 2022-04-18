#pragma once
#include "protocol.pb.h"
#include "types.h"
#include <google/protobuf/util/json_util.h>
#include "debug_helpers.h"
#include <string>

namespace yrclient {

// see issue #1
constexpr auto RESPONSE_OK = yrclient::Response::OK;
constexpr auto RESPONSE_ERROR = yrclient::Response::ERROR;

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

}  // namespace yrclient
