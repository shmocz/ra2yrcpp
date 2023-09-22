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

namespace pb = google::protobuf;

struct MessageBuilder {
  pb::DynamicMessageFactory F;
  const pb::DescriptorPool* pool;
  const pb::Descriptor* desc;
  pb::Message* m;
  const pb::Reflection* refl;
  explicit MessageBuilder(const std::string name);
};

struct MessageStream {
  explicit MessageStream(bool gzip);
  bool gzip;
};

struct MessageIstream : public MessageStream {
  MessageIstream(std::shared_ptr<std::istream> is, bool gzip);
  bool read(pb::Message* M);

  std::shared_ptr<std::istream> is;
  std::shared_ptr<pb::io::ZeroCopyInputStream> s_i;
  std::shared_ptr<pb::io::GzipInputStream> s_ig;
};

struct MessageOstream : public MessageStream {
  MessageOstream(std::shared_ptr<std::ostream> os, bool gzip);
  bool write(const pb::Message& M);

  std::shared_ptr<std::ostream> os;
  std::shared_ptr<pb::io::ZeroCopyOutputStream> s_o;
  std::shared_ptr<pb::io::GzipOutputStream> s_g;
};

bool write_message(const pb::Message* M, pb::io::CodedOutputStream* os);
bool read_message(pb::Message* M, pb::io::CodedInputStream* os);

/// Dynamically set the field "args" of B's Message by parsing the JSON string
/// in args. If args is empty, field is not set.
pb::Message* create_command_message(MessageBuilder* B, const std::string args);

/// Process stream of serialized protobuf messages of same type.
///
/// @param path Path to the file
/// @param M Message type to be read
/// @param cb Callback to invoke for each processed message. If unspecified,
/// dumps messages as JSON to stdout
void dump_messages(const std::string path, const pb::Message& M,
                   std::function<void(pb::Message*)> cb = nullptr);

std::string message_type(const pb::Any& m);
std::string message_type(const pb::Message& m);

bool from_json(const vecu8& bytes, pb::Message* m);
std::string to_json(const pb::Message& m);

template <typename T>
T from_any(const pb::Any& a) {
  T res;
  if (!a.UnpackTo(&res)) {
    throw std::runtime_error(
        fmt::format("Could not unpack message from {} to {}", a.type_url(),
                    message_type(res)));
  }
  return res;
}

std::vector<const pb::FieldDescriptor*> find_set_fields(const pb::Message& M);

///
/// Clears the RepeatedPtField and fills it with n copies of given type.
///
template <typename T>
void fill_repeated_empty(pb::RepeatedPtrField<T>* dst, const std::size_t n) {
  dst->Clear();
  for (auto i = 0U; i < n; i++) {
    dst->Add();
  }
}

void copy_field(pb::Message* dst, pb::Message* src,
                const pb::FieldDescriptor* f);

}  // namespace ra2yrcpp::protocol
