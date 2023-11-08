#pragma once

#include "ra2yrproto/core.pb.h"

#include "types.h"

namespace ra2yrcpp {

namespace gpb = google::protobuf;

constexpr auto RESPONSE_OK = ra2yrproto::ResponseCode::OK;
constexpr auto RESPONSE_ERROR = ra2yrproto::ResponseCode::ERROR;

/// Serialize message to vecu8
/// @param msg
/// @exception yrclient::protocol_error on serialization failure
vecu8 to_vecu8(const gpb::Message& msg);

ra2yrproto::Response make_response(
    const gpb::Message&& body,
    const ra2yrproto::ResponseCode code = RESPONSE_OK);

///
/// Create command message.
/// @param cmd message to be set as command field
/// @param type command type
/// @exception yrclient::protocol_error if message packing fails
///
ra2yrproto::Command create_command(
    const gpb::Message& cmd,
    ra2yrproto::CommandType type = ra2yrproto::CLIENT_COMMAND);

}  // namespace ra2yrcpp
