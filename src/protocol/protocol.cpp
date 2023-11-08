#include "protocol/protocol.hpp"

#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/commands_yr.pb.h"

#include "errors.hpp"
#include "protocol/helpers.hpp"

#include <fmt/core.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

#include <stdexcept>
#include <string>

using namespace ra2yrcpp;

vecu8 ra2yrcpp::to_vecu8(const gpb::Message& msg) {
  vecu8 res;
  res.resize(msg.ByteSizeLong());
  if (!msg.SerializeToArray(res.data(), res.size())) {
    throw ra2yrcpp::protocol_error(
        fmt::format("failed to serialize message {}", msg.GetTypeName()));
  }
  return res;
}

ra2yrproto::Response ra2yrcpp::make_response(
    const gpb::Message&& body, const ra2yrproto::ResponseCode code) {
  ra2yrproto::Response r;
  r.set_code(code);
  if (!r.mutable_body()->PackFrom(body)) {
    throw std::runtime_error("Could not pack message body");
  }
  return r;
}

ra2yrproto::Command ra2yrcpp::create_command(const gpb::Message& cmd,
                                             ra2yrproto::CommandType type) {
  ra2yrproto::Command C;
  C.set_command_type(type);
  if (!C.mutable_command()->PackFrom(cmd)) {
    throw ra2yrcpp::protocol_error("packing message failed");
  }
  return C;
}
