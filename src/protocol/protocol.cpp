#include "protocol/protocol.hpp"

#include "util_string.hpp"

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

static int get_fileno(FILE* fp) {
#ifdef _WIN32
  return _fileno(fp);
#else
  return fileno(fp);
#endif
}

// TODO(shmocz): fix warning about deprecated fileno on clang
CompressedOutputStream::CompressedOutputStream(const std::string path)
    : fd(std::unique_ptr<FILE, void (*)(FILE*)>(fopen(path.c_str(), "wb"),
                                                [](FILE* fp) { fclose(fp); })),
      s_fo(get_fileno(fd.get())),
      s_g(&s_fo) {}

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
