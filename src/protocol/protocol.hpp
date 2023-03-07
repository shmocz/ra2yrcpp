#pragma once
#include "types.h"
#include "util_string.hpp"

#include <cstdio>

#include <fstream>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <map>
#include <memory>
#include <ra2yrproto/commands_builtin.pb.h>
#include <ra2yrproto/commands_yr.pb.h>
#include <ra2yrproto/core.pb.h>
#include <ra2yrproto/game.pb.h>
#include <string>

namespace yrclient {

// see issue #1
constexpr auto RESPONSE_OK = ra2yrproto::ResponseCode::OK;
constexpr auto RESPONSE_ERROR = ra2yrproto::ResponseCode::ERROR;

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
ra2yrproto::Response make_response(
    const google::protobuf::Message&& body,
    const ra2yrproto::ResponseCode code = RESPONSE_OK);

template <typename T>
T from_any(const google::protobuf::Any& a) {
  T res;
  if (!a.UnpackTo(&res)) {
    throw std::runtime_error(std::string("Could not unpack message ") +
                             message_type(res));
  }
  return res;
}

struct CompressedOutputStream {
  std::unique_ptr<FILE, void (*)(FILE*)> fd;
  google::protobuf::io::FileOutputStream s_fo;
  google::protobuf::io::GzipOutputStream s_g;
  explicit CompressedOutputStream(const std::string path);
};

struct MessageBuilder {
  google::protobuf::DynamicMessageFactory F;
  const google::protobuf::DescriptorPool* pool;
  const google::protobuf::Descriptor* desc;
  google::protobuf::Message* m;
  const google::protobuf::Reflection* refl;
  explicit MessageBuilder(const std::string name);
};

bool write_message(const google::protobuf::Message* M,
                   google::protobuf::io::CodedOutputStream* os);
bool read_message(google::protobuf::Message* M,
                  google::protobuf::io::CodedInputStream* os);

/// Dynamically set the field "args" of B's Message by parsing the JSON string
/// in args. If args is empty, field is not set.
google::protobuf::Message* create_command_message(MessageBuilder* B,
                                                  const std::string args);

}  // namespace yrclient
