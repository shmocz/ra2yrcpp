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
#include <google/protobuf/repeated_ptr_field.h>

#include <cstddef>

#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ra2yrcpp::protocol {

namespace gpb = google::protobuf;

struct MessageBuilder {
  gpb::DynamicMessageFactory F;
  const gpb::DescriptorPool* pool;
  const gpb::Descriptor* desc;
  gpb::Message* m;
  const gpb::Reflection* refl;
  explicit MessageBuilder(const std::string name);
};

struct MessageStream {
  explicit MessageStream(bool gzip);
  bool gzip;
};

struct MessageIstream : public MessageStream {
  MessageIstream(std::shared_ptr<std::istream> is, bool gzip);
  bool read(gpb::Message* M);

  std::shared_ptr<std::istream> is;
  std::shared_ptr<gpb::io::ZeroCopyInputStream> s_i;
  std::shared_ptr<gpb::io::GzipInputStream> s_ig;
};

struct MessageOstream : public MessageStream {
  MessageOstream(std::shared_ptr<std::ostream> os, bool gzip);
  bool write(const gpb::Message& M);

  std::shared_ptr<std::ostream> os;
  std::shared_ptr<gpb::io::ZeroCopyOutputStream> s_o;
  std::shared_ptr<gpb::io::GzipOutputStream> s_g;
};

bool write_message(const gpb::Message* M, gpb::io::CodedOutputStream* os);
bool read_message(gpb::Message* M, gpb::io::CodedInputStream* os);

/// Dynamically set the field "args" of B's Message by parsing the JSON string
/// in args. If args is empty, field is not set.
gpb::Message* create_command_message(MessageBuilder* B, const std::string args);

/// Process stream of serialized protobuf messages of same type.
///
/// @param path Path to the file
/// @param M Message type to be read
/// @param cb Callback to invoke for each processed message. If unspecified,
/// dumps messages as JSON to stdout
void dump_messages(const std::string path, const gpb::Message& M,
                   std::function<void(gpb::Message*)> cb = nullptr);

std::string message_type(const gpb::Any& m);
std::string message_type(const gpb::Message& m);

bool from_json(const vecu8& bytes, gpb::Message* m);
std::string to_json(const gpb::Message& m);

template <typename T>
T from_any(const gpb::Any& a) {
  T res;
  if (!a.UnpackTo(&res)) {
    throw std::runtime_error(
        fmt::format("Could not unpack message from {} to {}", a.type_url(),
                    message_type(res)));
  }
  return res;
}

std::vector<const gpb::FieldDescriptor*> find_set_fields(const gpb::Message& M);

///
/// Fills with n copies of given type.
///
template <typename T>
void fill_repeated(gpb::RepeatedPtrField<T>* dst, const std::size_t n) {
  for (std::size_t i = 0U; i < n; i++) {
    dst->Add();
  }
}

///
/// Clears the RepeatedPtField and fills it with n copies of given type.
///
template <typename T>
void fill_repeated_empty(gpb::RepeatedPtrField<T>* dst, const std::size_t n) {
  dst->Clear();
  fill_repeated(dst, n);
}

void copy_field(gpb::Message* dst, gpb::Message* src,
                const gpb::FieldDescriptor* f);

/// Truncate RepeatedPtrField to given size. If length of dst
/// is less than n, no truncation is performed.
///
/// @param dst
/// @param n size to truncate to
/// @return True if truncation was performed. False otherwise.
template <typename T>
bool truncate(gpb::RepeatedPtrField<T>* dst, int n) {
  if (n < dst->size()) {
    dst->DeleteSubrange(n, (dst->size() - n));
    return true;
  }
  return false;
}

}  // namespace ra2yrcpp::protocol
