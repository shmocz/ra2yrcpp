#include "protocol/protocol.hpp"

#include "debug_helpers.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "gtest/gtest.h"
#include "proto_manager.hpp"
#include "util_string.hpp"

#include <cstdio>

#include <iostream>
#include <memory>
#include <vector>

using google::protobuf::Any;

class ProtocolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    key = "key";
    val = "val";
  }

  yrclient::StoreValue get_storeval() {
    yrclient::StoreValue S;
    auto* a = S.mutable_args();
    a->set_key(key);
    a->set_value(val);
    return S;
  }
  std::string key, val;
};

TEST_F(ProtocolTest, ProtoManager) {
  proto_manager::ProtoManager P;
  yrclient::NewResult R;
  auto S = get_storeval();
  Any dest;
  R.set_code(yrclient::RESPONSE_OK);
  R.mutable_body()->PackFrom(S);
  P.add_handler(S.GetTypeName(), []() { return new decltype(S); });
  // R.body().descriptor()
  auto* msg = P.get_message(yrclient::message_type(R.body()));
  R.body().UnpackTo(msg);
  auto* refl = msg->GetReflection();
  auto* descr = msg->GetDescriptor();
  auto* args_fd = descr->FindFieldByName("args");
  const auto& args = refl->GetMessage(*msg, args_fd);
  DPRINTF("prot=%s\n", yrclient::to_json(*msg).c_str());
  DPRINTF("args=%s\n", yrclient::to_json(args).c_str());
}

TEST_F(ProtocolTest, GenericMessages) {
  yrclient::NewResult R;
  auto S = get_storeval();
  Any dest;
  dest.PackFrom(S);
  R.set_code(yrclient::RESPONSE_OK);
  R.mutable_body()->PackFrom(S);
  google::protobuf::DynamicMessageFactory F;
  auto prot = F.GetPrototype(R.body().descriptor())->New();
  R.body().UnpackTo(prot);
  DPRINTF("prot=%s\n", yrclient::to_json(*prot).c_str());
}
