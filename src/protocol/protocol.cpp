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

void yrclient::set_field(const google::protobuf::Reflection* refl,
                         google::protobuf::Message* msg,
                         const google::protobuf::FieldDescriptor* field,
                         const std::string value) {
  using CppType = google::protobuf::FieldDescriptor::CppType;
  switch (field->cpp_type()) {
    case CppType::CPPTYPE_BOOL:
      refl->SetBool(msg, field, std::stoi(value) != 0);
      break;
    case CppType::CPPTYPE_FLOAT:
      refl->SetDouble(msg, field, std::stof(value));
      break;
    case CppType::CPPTYPE_INT32:
      refl->SetInt32(msg, field, std::stoi(value));
      break;
    case CppType::CPPTYPE_INT64:
      refl->SetInt32(msg, field, std::stol(value));
      break;
    case CppType::CPPTYPE_UINT32:
      refl->SetUInt32(msg, field, std::stoul(value));
      break;
    case CppType::CPPTYPE_UINT64:
      refl->SetUInt64(msg, field, std::stoul(value));
      break;
    case CppType::CPPTYPE_DOUBLE:
      refl->SetDouble(msg, field, std::stod(value));
      break;
    case CppType::CPPTYPE_STRING:
      refl->SetString(msg, field, value);
      break;
    default:
      break;
  }
}

void yrclient::set_message_field(google::protobuf::Message* m,
                                 const std::string key,
                                 const std::string value) {
  auto* refl = m->GetReflection();
  auto* desc = m->GetDescriptor();
  auto* fld = desc->FindFieldByName(key);

  set_field(refl, m, fld, value);
}

google::protobuf::Message* yrclient::create_command_message(
    const std::string name, google::protobuf::DynamicMessageFactory* F,
    const std::map<std::string, std::string> args) {
  auto* pool = google::protobuf::DescriptorPool::generated_pool();
  auto* desc = pool->FindMessageTypeByName(name);
  if (desc == nullptr) {
    throw std::runtime_error(name);
  }
  auto* msg_proto = F->GetPrototype(desc);
  auto* n = msg_proto->New();

  auto* reflection = n->GetReflection();
  auto* args_field = desc->FindFieldByName("args");
  auto* cmd_args = reflection->MutableMessage(n, args_field);
  for (const auto& [k, v] : args) {
    set_message_field(cmd_args, k, v);
  }
  return n;
}
