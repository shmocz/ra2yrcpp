#include "protocol/protocol.hpp"

#include "util_string.hpp"

#include <google/protobuf/stubs/status.h>
#include <google/protobuf/util/json_util.h>

#include <cstdio>

using namespace yrclient;

vecu8 yrclient::to_vecu8(const google::protobuf::Message& msg) {
  vecu8 res;
  res.resize(msg.ByteSizeLong());
  if (!msg.SerializeToArray(res.data(), res.size())) {
    throw yrclient::protocol_error(
        fmt::format("failed to serialize message {}", msg.GetTypeName()));
  }
  return res;
}

bool yrclient::from_json(const vecu8& bytes, google::protobuf::Message* m) {
  auto s = yrclient::to_string(bytes);
  if (google::protobuf::util::JsonStringToMessage(s, m).ok()) {
    return true;
  }

  return false;
}

std::string yrclient::to_json(const google::protobuf::Message& m) {
  std::string res;
  google::protobuf::util::MessageToJsonString(m, &res);
  return res;
}

std::string yrclient::message_type(const google::protobuf::Any& m) {
  auto toks = yrclient::split_string(m.type_url(), "/");
  return toks.back();
}

std::string yrclient::message_type(const google::protobuf::Message& m) {
  return m.GetTypeName();
}

ra2yrproto::Response yrclient::make_response(
    const google::protobuf::Message&& body,
    const ra2yrproto::ResponseCode code) {
  ra2yrproto::Response r;
  r.set_code(code);
  if (!r.mutable_body()->PackFrom(body)) {
    throw std::runtime_error("Could not pack message body");
  }
  return r;
}

bool yrclient::write_message(const google::protobuf::Message* M,
                             google::protobuf::io::CodedOutputStream* is) {
  auto l = M->ByteSizeLong();
  is->WriteVarint32(l);
  return M->SerializeToCodedStream(is) && !is->HadError();
}

bool yrclient::read_message(google::protobuf::Message* M,
                            google::protobuf::io::CodedInputStream* is) {
  uint32_t length;
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
    throw std::runtime_error(name);
  }
  auto* msg_proto = F.GetPrototype(desc);
  m = msg_proto->New();
  refl = m->GetReflection();
}

google::protobuf::Message* yrclient::create_command_message(
    MessageBuilder* B, const std::string args) {
  if (!args.empty()) {
    auto* cmd_args = B->m->GetReflection()->MutableMessage(
        B->m, B->desc->FindFieldByName("args"));
    google::protobuf::util::JsonStringToMessage(args, cmd_args);
  }
  return B->m;
}

ra2yrproto::Command yrclient::create_command(
    const google::protobuf::Message& cmd, ra2yrproto::CommandType type) {
  ra2yrproto::Command C;
  C.set_command_type(type);
  if (!C.mutable_command()->PackFrom(cmd)) {
    throw yrclient::protocol_error("packing message failed");
  }
  return C;
}

std::vector<const google::protobuf::FieldDescriptor*> yrclient::find_set_fields(
    const google::protobuf::Message& M) {
  auto* rfl = M.GetReflection();
  std::vector<const google::protobuf::FieldDescriptor*> out;
  rfl->ListFields(M, &out);
  return out;
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
    return yrclient::write_message(&M, &co);
  } else {
    google::protobuf::io::CodedOutputStream co(s_o.get());
    return yrclient::write_message(&M, &co);
  }
  return false;
}

bool MessageIstream::read(google::protobuf::Message* M) {
  if (is == nullptr) {
    return false;
  }

  if (gzip) {
    google::protobuf::io::CodedInputStream co(s_ig.get());
    return yrclient::read_message(M, &co);
  } else {
    google::protobuf::io::CodedInputStream co(s_i.get());
    return yrclient::read_message(M, &co);
  }
  return false;
}

void yrclient::dump_messages(
    const std::string path, const google::protobuf::Message& M,
    std::function<void(google::protobuf::Message*)> cb) {
  bool ok = true;
  auto ii = std::make_shared<std::ifstream>(
      path, std::ios_base::in | std::ios_base::binary);
  yrclient::MessageBuilder B(M.GetTypeName());

  const bool use_gzip = true;
  yrclient::MessageIstream MS(ii, use_gzip);

  if (cb == nullptr) {
    cb = [](auto* M) { fmt::print("{}\n", yrclient::to_json(*M)); };
  }

  while (ok) {
    ok = MS.read(B.m);
    cb(B.m);
  }
}
