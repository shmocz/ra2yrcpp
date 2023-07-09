#pragma once
#include "errors.hpp"
#include "ra2yrproto/commands_builtin.pb.h"
#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/core.pb.h"
#include "ra2yrproto/game.pb.h"
#include "types.h"

#include <fmt/core.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/message.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace yrclient {

// see issue #1
constexpr auto RESPONSE_OK = ra2yrproto::ResponseCode::OK;
constexpr auto RESPONSE_ERROR = ra2yrproto::ResponseCode::ERROR;

/// Serialize message to vecu8
/// @param msg
/// @exception yrclient::protocol_error on serialization failure
vecu8 to_vecu8(const google::protobuf::Message& msg);

bool from_json(const vecu8& bytes, google::protobuf::Message* m);
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
    throw std::runtime_error(
        fmt::format("Could not unpack message from {} to {}", a.type_url(),
                    message_type(res)));
  }
  return res;
}

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

///
/// Create command message.
/// @param cmd message to be set as command field
/// @param type command type
/// @exception yrclient::protocol_error if message packing fails
///
ra2yrproto::Command create_command(
    const google::protobuf::Message& cmd,
    ra2yrproto::CommandType type = ra2yrproto::CLIENT_COMMAND);

std::vector<const google::protobuf::FieldDescriptor*> find_set_fields(
    const google::protobuf::Message& M);

///
/// Clears the RepeatedPtField and fills it with n copies of given type.
///
template <typename T>
void fill_repeated_empty(google::protobuf::RepeatedPtrField<T>* dst,
                         const std::size_t n) {
  dst->Clear();
  for (auto i = 0U; i < n; i++) {
    dst->Add();
  }
}

struct MessageStream {
  explicit MessageStream(bool gzip);
  bool gzip;
};

struct MessageIstream : public MessageStream {
  MessageIstream(std::shared_ptr<std::istream> is, bool gzip);
  bool read(google::protobuf::Message* M);

  std::shared_ptr<std::istream> is;
  std::shared_ptr<google::protobuf::io::IstreamInputStream> s_i;
  std::shared_ptr<google::protobuf::io::GzipInputStream> s_ig;
};

struct MessageOstream : public MessageStream {
  MessageOstream(std::shared_ptr<std::ostream> os, bool gzip);
  bool write(const google::protobuf::Message& M);

  std::shared_ptr<std::ostream> os;
  std::shared_ptr<google::protobuf::io::OstreamOutputStream> s_o;
  std::shared_ptr<google::protobuf::io::GzipOutputStream> s_g;
};

/// Process stream of serialized protobuf messages of same type.
///
/// @param path Path to the file
/// @param M Message type to be read
/// @param cb Callback to invoke for each processed message. If unspecified,
/// dumps messages as JSON to stdout
void dump_messages(
    const std::string path, const google::protobuf::Message& M,
    std::function<void(google::protobuf::Message*)> cb = nullptr);

}  // namespace yrclient
