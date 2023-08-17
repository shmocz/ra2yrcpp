#include "protocol/helpers.hpp"

#include "types.h"
#include "util_string.hpp"

#include <google/protobuf/any.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#undef GetMessage

using namespace ra2yrcpp::protocol;

bool ra2yrcpp::protocol::write_message(
    const google::protobuf::Message* M,
    google::protobuf::io::CodedOutputStream* is) {
  auto l = M->ByteSizeLong();
  is->WriteVarint32(l);
  return M->SerializeToCodedStream(is) && !is->HadError();
}

bool ra2yrcpp::protocol::read_message(
    google::protobuf::Message* M, google::protobuf::io::CodedInputStream* is) {
  u32 length;
  if (!is->ReadVarint32(&length)) {
    return false;
  }
  auto l = is->PushLimit(length);
  bool res = M->ParseFromCodedStream(is);
  is->PopLimit(l);
  return res;
}

MessageBuilder::MessageBuilder(const std::string name) {
  pool = google::protobuf::DescriptorPool::generated_pool();
  desc = pool->FindMessageTypeByName(name);
  if (desc == nullptr) {
    throw std::runtime_error(std::string("no such message ") + name);
  }
  auto* msg_proto = F.GetPrototype(desc);
  m = msg_proto->New();
  refl = m->GetReflection();
}

// TODO: ensure that this works OK for nullptr stream
MessageIstream::MessageIstream(std::shared_ptr<std::istream> is, bool gzip)
    : MessageStream(gzip),
      is(is),
      s_i(std::make_shared<google::protobuf::io::IstreamInputStream>(
          is.get())) {
  if (gzip) {
    s_ig = std::make_shared<google::protobuf::io::GzipInputStream>(s_i.get());
  }
}

MessageStream::MessageStream(bool gzip) : gzip(gzip) {}

MessageOstream::MessageOstream(std::shared_ptr<std::ostream> os, bool gzip)
    : MessageStream(gzip), os(os) {
  if (os == nullptr) {
    return;
  }
  s_o = std::make_shared<google::protobuf::io::OstreamOutputStream>(os.get());
  if (gzip) {
    s_g = std::make_shared<google::protobuf::io::GzipOutputStream>(s_o.get());
  }
}

bool MessageOstream::write(const google::protobuf::Message& M) {
  if (os == nullptr) {
    return false;
  }

  if (gzip) {
    google::protobuf::io::CodedOutputStream co(s_g.get());
    return write_message(&M, &co);
  } else {
    google::protobuf::io::CodedOutputStream co(s_o.get());
    return write_message(&M, &co);
  }
  return false;
}

bool MessageIstream::read(google::protobuf::Message* M) {
  if (is == nullptr) {
    return false;
  }

  if (gzip) {
    google::protobuf::io::CodedInputStream co(s_ig.get());
    return read_message(M, &co);
  } else {
    google::protobuf::io::CodedInputStream co(s_i.get());
    return read_message(M, &co);
  }
  return false;
}

google::protobuf::Message* ra2yrcpp::protocol::create_command_message(
    MessageBuilder* B, const std::string args) {
  if (!args.empty()) {
    auto* cmd_args = B->m->GetReflection()->MutableMessage(
        B->m, B->desc->FindFieldByName("args"));
    google::protobuf::util::JsonStringToMessage(args, cmd_args);
  }
  return B->m;
}

void ra2yrcpp::protocol::dump_messages(
    const std::string path, const google::protobuf::Message& M,
    std::function<void(google::protobuf::Message*)> cb) {
  bool ok = true;
  auto ii = std::make_shared<std::ifstream>(
      path, std::ios_base::in | std::ios_base::binary);
  ra2yrcpp::protocol::MessageBuilder B(M.GetTypeName());

  const bool use_gzip = true;
  ra2yrcpp::protocol::MessageIstream MS(ii, use_gzip);

  if (cb == nullptr) {
    cb = [](auto* M) { fmt::print("{}\n", ra2yrcpp::protocol::to_json(*M)); };
  }

  while (ok) {
    ok = MS.read(B.m);
    cb(B.m);
  }
}

std::string ra2yrcpp::protocol::message_type(const google::protobuf::Any& m) {
  auto toks = yrclient::split_string(m.type_url(), "/");
  return toks.back();
}

std::string ra2yrcpp::protocol::message_type(
    const google::protobuf::Message& m) {
  return m.GetTypeName();
}

bool ra2yrcpp::protocol::from_json(const vecu8& bytes,
                                   google::protobuf::Message* m) {
  auto s = yrclient::to_string(bytes);
  if (google::protobuf::util::JsonStringToMessage(s, m).ok()) {
    return true;
  }

  return false;
}

std::string ra2yrcpp::protocol::to_json(const google::protobuf::Message& m) {
  std::string res;
  google::protobuf::util::MessageToJsonString(m, &res);
  return res;
}

std::vector<const google::protobuf::FieldDescriptor*>
ra2yrcpp::protocol::find_set_fields(const google::protobuf::Message& M) {
  auto* rfl = M.GetReflection();
  std::vector<const google::protobuf::FieldDescriptor*> out;
  rfl->ListFields(M, &out);
  return out;
}

void ra2yrcpp::protocol::copy_field(
    google::protobuf::Message* dst, google::protobuf::Message* src,
    const google::protobuf::FieldDescriptor* f) {
  dst->GetReflection()->MutableMessage(dst, f)->CopyFrom(
      src->GetReflection()->GetMessage(*src, f));
}
