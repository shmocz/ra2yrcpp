#pragma once

#include "ra2yrproto/core.pb.h"

#include "types.h"

namespace yrclient {

constexpr auto RESPONSE_OK = ra2yrproto::ResponseCode::OK;
constexpr auto RESPONSE_ERROR = ra2yrproto::ResponseCode::ERROR;

/// Serialize message to vecu8
/// @param msg
/// @exception yrclient::protocol_error on serialization failure
vecu8 to_vecu8(const google::protobuf::Message& msg);

ra2yrproto::Response make_response(
    const google::protobuf::Message&& body,
    const ra2yrproto::ResponseCode code = RESPONSE_OK);

///
/// Create command message.
/// @param cmd message to be set as command field
/// @param type command type
/// @exception yrclient::protocol_error if message packing fails
///
ra2yrproto::Command create_command(
    const google::protobuf::Message& cmd,
    ra2yrproto::CommandType type = ra2yrproto::CLIENT_COMMAND);

}  // namespace yrclient
