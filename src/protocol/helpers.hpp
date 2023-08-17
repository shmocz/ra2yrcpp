#pragma once

#include "types.h"

#include <fmt/core.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>

#include <cstddef>

#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ra2yrcpp::protocol {

struct MessageBuilder {
  google::protobuf::DynamicMessageFactory F;
  const google::protobuf::DescriptorPool* pool;
  const google::protobuf::Descriptor* desc;
  google::protobuf::Message* m;
  const google::protobuf::Reflection* refl;
  explicit MessageBuilder(const std::string name);
};

struct MessageStream {
  explicit MessageStream(bool gzip);
  bool gzip;
};

struct MessageIstream : public MessageStream {
  MessageIstream(std::shared_ptr<std::istream> is, bool gzip);
  bool read(google::protobuf::Message* M);

  std::shared_ptr<std::istream> is;
  std::shared_ptr<google::protobuf::io::ZeroCopyInputStream> s_i;
  std::shared_ptr<google::protobuf::io::GzipInputStream> s_ig;
};

struct MessageOstream : public MessageStream {
  MessageOstream(std::shared_ptr<std::ostream> os, bool gzip);
  bool write(const google::protobuf::Message& M);

  std::shared_ptr<std::ostream> os;
  std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> s_o;
  std::shared_ptr<google::protobuf::io::GzipOutputStream> s_g;
};

bool write_message(const google::protobuf::Message* M,
                   google::protobuf::io::CodedOutputStream* os);
bool read_message(google::protobuf::Message* M,
                  google::protobuf::io::CodedInputStream* os);

/// Dynamically set the field "args" of B's Message by parsing the JSON string
/// in args. If args is empty, field is not set.
google::protobuf::Message* create_command_message(MessageBuilder* B,
                                                  const std::string args);

/// Process stream of serialized protobuf messages of same type.
///
/// @param path Path to the file
/// @param M Message type to be read
/// @param cb Callback to invoke for each processed message. If unspecified,
/// dumps messages as JSON to stdout
void dump_messages(
    const std::string path, const google::protobuf::Message& M,
    std::function<void(google::protobuf::Message*)> cb = nullptr);

std::string message_type(const google::protobuf::Any& m);
std::string message_type(const google::protobuf::Message& m);

bool from_json(const vecu8& bytes, google::protobuf::Message* m);
std::string to_json(const google::protobuf::Message& m);

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

void copy_field(google::protobuf::Message* dst, google::protobuf::Message* src,
                const google::protobuf::FieldDescriptor* f);

}  // namespace ra2yrcpp::protocol
