#include "protocol/protocol.hpp"

using namespace yrclient;

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

yrclient::Response yrclient::make_response(
    const yrclient::ResponseCode code, const google::protobuf::Message& body) {
  yrclient::Response r;
  r.set_code(code);
  if (!r.mutable_body()->PackFrom(body)) {
    throw std::runtime_error("Could not pack message body");
  }
  return r;
}

CompressedOutputStream::CompressedOutputStream(const std::string path)
    : os(path, std::ios_base::out | std::ios_base::binary),
      s_f(&os),
      s_g(&s_f) {}

bool yrclient::write_message(google::protobuf::Message* M,
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
