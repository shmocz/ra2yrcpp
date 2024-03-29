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

bool ra2yrcpp::protocol::write_message(const gpb::Message* M,
                                       gpb::io::CodedOutputStream* is) {
  auto l = M->ByteSizeLong();
  is->WriteVarint32(l);
  return M->SerializeToCodedStream(is) && !is->HadError();
}

bool ra2yrcpp::protocol::read_message(gpb::Message* M,
                                      gpb::io::CodedInputStream* is) {
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
  pool = gpb::DescriptorPool::generated_pool();
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
      s_i(std::make_shared<gpb::io::IstreamInputStream>(is.get())) {
  if (gzip) {
    s_ig = std::make_shared<gpb::io::GzipInputStream>(s_i.get());
  }
}

MessageStream::MessageStream(bool gzip) : gzip(gzip) {}

MessageOstream::MessageOstream(std::shared_ptr<std::ostream> os, bool gzip)
    : MessageStream(gzip), os(os) {
  if (os == nullptr) {
    return;
  }
  s_o = std::make_shared<gpb::io::OstreamOutputStream>(os.get());
  if (gzip) {
    s_g = std::make_shared<gpb::io::GzipOutputStream>(s_o.get());
  }
}

bool MessageOstream::write(const gpb::Message& M) {
  if (os == nullptr) {
    return false;
  }

  if (gzip) {
    gpb::io::CodedOutputStream co(s_g.get());
    return write_message(&M, &co);
  } else {
    gpb::io::CodedOutputStream co(s_o.get());
    return write_message(&M, &co);
  }
  return false;
}

bool MessageIstream::read(gpb::Message* M) {
  if (is == nullptr) {
    return false;
  }

  if (gzip) {
    gpb::io::CodedInputStream co(s_ig.get());
    return read_message(M, &co);
  } else {
    gpb::io::CodedInputStream co(s_i.get());
    return read_message(M, &co);
  }
  return false;
}

gpb::Message* ra2yrcpp::protocol::create_command_message(
    MessageBuilder* B, const std::string args) {
  if (!args.empty()) {
    auto* cmd_args = B->m->GetReflection()->MutableMessage(
        B->m, B->desc->FindFieldByName("args"));
    gpb::util::JsonStringToMessage(args, cmd_args);
  }
  return B->m;
}

void ra2yrcpp::protocol::dump_messages(const std::string path,
                                       const gpb::Message& M,
                                       std::function<void(gpb::Message*)> cb) {
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

std::string ra2yrcpp::protocol::message_type(const gpb::Any& m) {
  auto toks = ra2yrcpp::split_string(m.type_url(), "/");
  return toks.back();
}

std::string ra2yrcpp::protocol::message_type(const gpb::Message& m) {
  return m.GetTypeName();
}

bool ra2yrcpp::protocol::from_json(const vecu8& bytes, gpb::Message* m) {
  auto s = ra2yrcpp::to_string(bytes);
  if (gpb::util::JsonStringToMessage(s, m).ok()) {
    return true;
  }

  return false;
}

std::string ra2yrcpp::protocol::to_json(const gpb::Message& m) {
  std::string res;
  gpb::util::MessageToJsonString(m, &res);
  return res;
}

std::vector<const gpb::FieldDescriptor*> ra2yrcpp::protocol::find_set_fields(
    const gpb::Message& M) {
  auto* rfl = M.GetReflection();
  std::vector<const gpb::FieldDescriptor*> out;
  rfl->ListFields(M, &out);
  return out;
}

void ra2yrcpp::protocol::copy_field(gpb::Message* dst, gpb::Message* src,
                                    const gpb::FieldDescriptor* f) {
  dst->GetReflection()->MutableMessage(dst, f)->CopyFrom(
      src->GetReflection()->GetMessage(*src, f));
}
